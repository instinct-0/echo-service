#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <poll.h>

#include "server.h"


#ifdef DEBUG

#define LOG_DEBUG(msg) \
    printf("[DEBUG] %s (%s:%i)\n", msg, __FILE__, __LINE__)

#define LOG_ERROR(msg) \
    fprintf(stderr, "[ERROR] %s %s\n", msg, (errno != 0 ? strerror(errno) : " "))
#else

#define LOG_DEBUG(msg) (void)0;
#define LOG_ERROR(msg) (void)0;

#endif

volatile sig_atomic_t keep_running = 1;

const struct socket_config server_socket_config[] = {
    {AF_INET, SOCK_STREAM},
#ifdef ENABLE_UDP
    {AF_INET, SOCK_DGRAM},
#endif
    
#ifdef ENABLE_IPV6
    {AF_INET6, SOCK_STREAM},
#ifdef ENABLE_UDP
    {AF_INET6, SOCK_DGRAM}
#endif
#endif
};

/**
 * @def NSERVER\_SOCKETS
 * Number of server sockets.
 */
#define NSERVER_SOCKETS \
    sizeof(server_socket_config)/sizeof(struct socket_config)
const int nserver_sockets = NSERVER_SOCKETS;


/**
 * @struct client\_connection
 */
struct client_connection {
    void *buffer;
    int *fd; /*!< Client socket fd. */
    size_t len; /*!< Request length. */
    size_t sent;
};

struct client_connection clients[MAX_CLIENTS];
int nconnections = 0; /*!< Number of connections. */


/**
 * @def POLLFDS\_LEN
 */
#define POLLFDS_LEN \
    NSERVER_SOCKETS + MAX_CLIENTS

const int pollfds_len = POLLFDS_LEN;
struct pollfd pollfds[POLLFDS_LEN];



/**
 * @brief Handle signals.
 */
void server_signal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
        keep_running = 0;
}


/**
 * @brief Creates a new server socket.
 * This function returns a ready to accept socket.
 *
 * @param[in] config Socket config.
 * @return If success, a new socket, otherwise -1.
 */
int server_new_socket(struct socket_config config)
{
    struct sockaddr_storage *addr;
#ifdef ENABLE_IPV6
    struct sockaddr_in6 addr6;
#endif
    struct sockaddr_in addr4;
    socklen_t addr_len;
    
    int fd = -1;
    int protocol = config.type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;
    int sockopt_on = 1;
    int fcntl_flags;

    
    if (config.family != AF_INET
#ifdef ENABLE_IPV6
            && config.family != AF_INET6
#endif
    ) {
        LOG_ERROR("Invalid socket family");
        return -1;

    } else if (config.type != SOCK_STREAM
#ifdef ENABLE_UDP
             && config.type != SOCK_DGRAM
#endif
    ) {
        LOG_ERROR("Invalid socket type");
        return -1;
    }

    if ((fd = socket(config.family, config.type, protocol)) == -1) {
        LOG_ERROR("Failed to create a socket");
        goto error;
        
    } else if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) == -1 ||
        (config.family == AF_INET6 && setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &sockopt_on, sizeof(sockopt_on)) == -1)) {
        LOG_ERROR("Failed to set setsockopt");
        goto error;
        
    } else if ((fcntl_flags = fcntl(fd, F_GETFL)) == -1 ||
            fcntl(fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1)
    {
        LOG_ERROR("Failed to fcntl");
        goto error;
    }


    if (config.family == AF_INET) {
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(LISTEN_PORT);
        addr4.sin_addr.s_addr = INADDR_ANY;
        
        addr = (struct sockaddr_storage*)&addr4;
        addr_len = sizeof(struct sockaddr_in);
    }
#ifdef ENABLE_IPV6
    else {
        memset(&addr6, 0, sizeof(struct sockaddr_in6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(LISTEN_PORT);

        addr = (struct sockaddr_storage*)&addr6;
        addr_len = sizeof(addr6);
    }
#endif
    

    if (bind(fd, (struct sockaddr*)addr, addr_len) == -1) {
        LOG_ERROR("Failed to bind socket");
        goto error;
    }

    if (protocol == IPPROTO_TCP) {
        if (listen(fd, LISTEN_BACKLOG) == -1) {
            LOG_ERROR("Failed to listen");
            goto error;
        }
    }

    return fd;
error:
    if (fd != -1)
        close(fd);

    return -1;
}


/**
 * @brief Initialize server variables.
 */
void server_init(void)
{
    int i;

    
    memset(pollfds, 0, sizeof(struct pollfd)*pollfds_len);
    /* Initialize server */
    for (i = 0; i < nserver_sockets; i++) {
        struct socket_config config = server_socket_config[i];

        if ((pollfds[i].fd = server_new_socket(config)) == -1)
            exit(1);

        pollfds[i].events = POLLIN;
    }

    /* Initialize client stuff */
    for (i = nserver_sockets; i < pollfds_len; i++) {
        int client_index = i - nserver_sockets;
        
        pollfds[i].fd = -1;
        pollfds[i].events = POLLIN;
        
        clients[client_index].fd = &pollfds[i].fd;
        clients[client_index].buffer = NULL;
        clients[client_index].len = 0;
        clients[client_index].sent = 0;
    }

    signal(SIGINT, server_signal);
    signal(SIGTERM, server_signal);
}


/**
 * @brief Gets client by @p fd .
 * It searches for a client by it's fd. If @p fd equals to -1, so it returns the first
 * free client.
 *
 * @param[in] fd Socket number.
 * @return If success, a pointer to a client\_connection, otherwise NULL.
 */
struct client_connection *server_get_client(int fd)
{
    unsigned i;

    
    if (nconnections == MAX_CLIENTS)
        return NULL;
    
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (*clients[i].fd == fd)
            return &clients[i];
    }

    return NULL;
}


/**
 * @brief Accept a new TCP client.
 *
 * @param[in] server_fd Server fd.
 * @param[out] client_conn Client connection.
 * @return If success 0, otherwise 1.
 */
int server_accept_tcp_client(int server_fd, struct client_connection *client_conn)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(struct sockaddr_storage);
    int fcntl_flags;


    if (nconnections == MAX_CLIENTS)
        return 1;
    
    if (client_conn == NULL) {
        LOG_ERROR("Invalid client_conn pointer");
        return 1;
    }

    client_conn->len = 0;
    client_conn->sent = 0;
    if (client_conn->buffer == NULL && (client_conn->buffer = malloc(BUFFER_SIZE)) == NULL) {
        LOG_ERROR("Failed to allocate data");
        return 1;

    } else if ((*client_conn->fd= accept(server_fd, (struct sockaddr*)&addr, &addr_len)) == -1) {
        LOG_ERROR("Failed to accept client");
        return 1;
        
    } else if ((fcntl_flags = fcntl(*client_conn->fd, F_GETFL)) == -1 ||
           fcntl(*client_conn->fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1)
    {
        LOG_ERROR("Failed to fcntl");

        close(*client_conn->fd);
        *client_conn->fd = -1;
        return 1;
    }

    nconnections++;
    return 0;
}


/**
 * @brief Receive TCP request.
 *
 * @param[out] client_conn Client connection.
 * @return -1 Error.
 * @return 0 Close connection
 * @return 1 Ready to send data.
 * @return 2 Try again.
 */
int server_recv_tcp_data(struct client_connection *client_conn)
{
    if (client_conn == NULL) {
        LOG_ERROR("Invalid client_conn pointer");
        return -1;
    }
    
    if ((client_conn->len = recv(*client_conn->fd, client_conn->buffer, BUFFER_SIZE, 0)) == (size_t)-1) {
        if (errno == EAGAIN)
            return 2;
        
        LOG_ERROR("Failed to receive TCP data");
        return -1;
        
    } else if (client_conn->len == 0) {
        LOG_DEBUG("Client closed connection");
        return 0;
    }
    
    return 1;
}


/**
 * @brief Echo request to the client.
 *
 * @param[in] client_conn Client connection.
 * @return -1 Error.
 * @return 0 Close connection.
 * @return 1 Ready to receive data.
 * @return 2 Try again.
 */
int server_echo_tcp_data(struct client_connection *client_conn)
{
    if (client_conn == NULL) {
        LOG_ERROR("Invalid client_conn pointer");
        return -1;

    } else if (client_conn->sent >= client_conn->len) {
        client_conn->len = 0;
        client_conn->sent = 0;
        return 1;
    }

    do {
        void *buffer = (char*)client_conn->buffer + client_conn->sent;
        size_t bytes_sent, remaining_bytes = client_conn->len - client_conn->sent;

        bytes_sent = send(*client_conn->fd, buffer, remaining_bytes, MSG_NOSIGNAL);
        if (bytes_sent == (size_t)-1) {
            if (errno == EAGAIN)
                return 2;
            
            LOG_ERROR("Failed to send TCP data");
            return -1;
            
        } else if (bytes_sent == 0) {
            LOG_DEBUG("Client closed connection");
            return 0;
        }
        
        client_conn->sent += bytes_sent;
    } while (client_conn->sent < client_conn->len);

    client_conn->len = 0;
    client_conn->sent = 0;
    return 1;
}


/**
 * @brief Close client connection.
 *
 * @param[in] client_conn Client connection.
 */
void server_close_tcp_connection(struct client_connection *client_conn)
{
    if (nconnections == 0)
        return;
    
    if (client_conn == NULL) {
        LOG_ERROR("Invalid client_conn pointer");
        return;
    }
    
    if (*client_conn->fd != -1) {
        shutdown(*client_conn->fd, SHUT_RDWR);
        close(*client_conn->fd);
    }
    
    *client_conn->fd = -1;
    if (client_conn->buffer != NULL) {
        free(client_conn->buffer);
        client_conn->buffer = NULL;
    }
    
    nconnections--;
}


#ifdef ENABLE_UDP
/**
 * @brief Receive and echo UDP data.
 *
 * @param[in] server_fd Server socket that's receiving UDP data.
 */
void server_echo_udp_data(int server_fd)
{
    char buffer[BUFFER_SIZE];
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(struct sockaddr_storage);
    ssize_t bytes;


    if ((bytes = recvfrom(server_fd, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&addr, &addr_len)) == -1) {
        LOG_ERROR("Failed to receive UDP data");
        return;
    } else if (sendto(server_fd, buffer, bytes, 0, (struct sockaddr*)&addr, addr_len) <= 0) {
        LOG_ERROR("Failed to send UDP echo");
        return;
    }
}
#endif


/**
 * @brief Start accepting connections
 */
void server_start(void)
{
    server_init();

    printf("[*] Echo Service running (control + c to stop)\n");
    while (keep_running) {
        int i, nevents = poll((struct pollfd*)&pollfds, pollfds_len, -1);

        if (nevents < 1)
            continue;

        for (i = 0; i < pollfds_len; i++) {
            int fd = pollfds[i].fd;
            int revents = pollfds[i].revents;

            if (revents == 0)
                continue;

            if (i < nserver_sockets) {
                struct socket_config server_config = server_socket_config[i];

                LOG_DEBUG("Server event");
                if (revents & POLLIN) {
                    if (server_config.type == SOCK_STREAM) {
                        server_accept_tcp_client(fd, server_get_client(-1));
                        pollfds[i].events = POLLIN;
                    }
#ifdef ENABLE_UDP
                    else {
                        server_echo_udp_data(fd);
                    }
#endif
                }
            } /* Server events */
            else {
                struct client_connection *conn = &clients[i - nserver_sockets];

                if (revents & POLLIN) {
                    switch (server_recv_tcp_data(conn)) {
                        case -1: /* Error */
                        case 0: /* Connection closed */
                            server_close_tcp_connection(conn);
                            continue;

                        case 1: /* Ready to send data */
                            pollfds[i].events = POLLOUT;
                            break;
                            
                        case 2: /* EAGAIN */
                            break;
                    }
                }

                if (revents & POLLOUT) {
                    switch (server_echo_tcp_data(conn)) {
                        case -1: /* Error */
                        case 0: /* Connection closed */
                            server_close_tcp_connection(conn);
                            pollfds[i].events = 0;
                            continue;
                            
                        case 1: /* Ready to receive data */
                            pollfds[i].events = POLLIN;
                            break;
                            
                        case 2: /* EAGAIN */
                            break;
                    }
                } /* POLLOUT */
            } /* Client events */
        } /* nevents loop */
    }

    server_stop();
}


/**
 * @brief Stop server
 */
void server_stop(void)
{
    int i;

    printf("\n[*] Stopping server...\n");
    for (i = 0; i < pollfds_len; i++) {
        int fd = pollfds[i].fd;

        
        if (fd != -1) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
        
        if (i >= nserver_sockets) {
            if (clients[i - nserver_sockets].buffer != NULL) {
                free(clients[i - nserver_sockets].buffer);
                clients[i - nserver_sockets].buffer = NULL;
            }
            
            clients[i - nserver_sockets].len = 0;
            clients[i - nserver_sockets].sent = 0;
        }
    }
}
