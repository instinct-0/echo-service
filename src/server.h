#ifndef __PRIVATE_ECHO_SERVER__
#define __PRIVATE_ECHO_SERVER__

#ifndef LISTEN_PORT
#define LISTEN_PORT 7
#endif

#ifndef LISTEN_BACKLOG
#define LISTEN_BACKLOG 50
#endif

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 10
#endif

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 32
#endif


/**
 * @struct socket\_config
 */
struct socket_config {
    const int family;
    const int type;
};

extern int server_new_socket(struct socket_config);
extern void server_init(void);
extern void server_start(void);
extern void server_stop(void);

#endif
