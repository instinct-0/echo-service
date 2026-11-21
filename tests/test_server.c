#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <signal.h>
#include <sys/wait.h>
#include <poll.h>


#include "../src/server.h"


/**
 * @struct client\_connection
 */
struct client_connection {
    void *buffer;
    int *fd; /*!< Client socket fd. */
    size_t len; /*!< Request length. */
    size_t sent;
};


extern struct client_connection clients[];
extern const struct socket_config server_socket_config[];
extern const int nserver_sockets;
extern const int pollfds_len;
extern struct pollfd pollfds[];


/**
 * @brief Connect socket to echo server.
 *
 */
static int connect_socket(int fd, int family)
{
    struct sockaddr *addr;
    struct sockaddr_in6 addr6;
    struct sockaddr_in addr4;
    socklen_t addr_len;
    
    if (family == AF_INET) {
        bzero(&addr4, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(LISTEN_PORT);
        addr4.sin_addr.s_addr = INADDR_ANY;

        addr = (struct sockaddr*)&addr4;
        addr_len = sizeof(addr4);

    } else if (family == AF_INET6) {
        bzero(&addr6, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(LISTEN_PORT);
        addr6.sin6_addr = in6addr_any;

        addr = (struct sockaddr*)&addr6;
        addr_len = sizeof(addr6);

    } else {
        fprintf(stderr, "[-] Invalid socket family\n");
        exit(1);
    }
    
    return connect(fd, addr, addr_len);
}


/**
 * @brief Test server initialization.
 */
void test_server_init(void)
{
    int i;

    server_init();
    for (i = 0; i < pollfds_len; i++) {
        if (pollfds[i].fd == 0) {
            fprintf(stderr, "[-] Failed to initialize socket: %i\n", i);
            exit(1);
        }
        
        if (i > nserver_sockets) {
            int client_index = i - nserver_sockets;
            
            if (clients[client_index].buffer != NULL ||
                clients[client_index].fd == NULL ||
                clients[client_index].len != 0 ||
                clients[client_index].sent != 0)
            {
                fprintf(stderr, "[-] Failed to initialize clients\n");
                exit(1);
            }
        }
    }
}


/**
 * @brief Tests server echo.
 */
void test_server_echo(void)
{
    char buffer[10];
    const char *request = "ABCD1234";
    ssize_t request_len = strlen(request);
    pid_t pid;
    int i;
    
    
    if ((pid = fork()) == 0) {
        server_start();
        exit(0);

    } else if (pid != -1 ) {
        sleep(1);
        for (i = 0; i < nserver_sockets; i++) {
            struct socket_config config = server_socket_config[i];
            int fd, protocol = config.type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;

            if ((fd = socket(config.family, config.type, protocol)) == -1) {
                fprintf(stderr, "[-] Failed to create a socket");
                goto error;
                
            } else if (connect_socket(fd, config.family) == -1) {
                fprintf(stderr, "[-] Failed to connect socket\n");
                close(fd);
                goto error;
                
            } else if (send(fd, request, request_len, 0) != request_len) {
                fprintf(stderr, "[-] Failed to send request\n");
                close(fd);
                goto error;
                
            } else if (recv(fd, buffer, request_len, 0) != request_len) {
                fprintf(stderr, "[-] Failed to receive echo\n");
                close(fd);
                goto error;
                
            } else if (strncmp(buffer, request, request_len)) {
                fprintf(stderr, "[-] Received invalid echo\n");
                close(fd);
                goto error;
            }
            
            close(fd);
        }
    }
    
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    return;
    
error:
    kill(pid, SIGINT);
    waitpid(pid, NULL, 0);
    exit(1);
}





int main(int argc, char **argv)
{
    int test_number = 0;
    
    if (argc == 2)
        test_number = atoi(argv[1]);

    switch (test_number) {
        default:
            fprintf(stderr, "[-] Invalid test number!\n");
            return 1;

        case 1:
            test_server_init();
            break;
            
        case 2:
            test_server_echo();
            break;
    }

    return 0;
}
