
#ifndef COORDINATOR_H_
#define COORDINATOR_H_

#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define COORDINATOR_PORT 4269
#define NREDUCE 3

// Network stuffs

#define MAX_EVENTS 42

typedef struct tcp_sock_s {
    struct sockaddr_in addr;
    int fd;
    // struct epoll_event *event;
} tcp_sock_t;

typedef struct clients_s {
    size_t count;
    size_t capacity;
    tcp_sock_t **items;
} client_t;

typedef struct network_context_s {
    tcp_sock_t *serv;
    client_t client_set;
    int epfd;
} network_ctx_t;

typedef int (*fptr_t)(network_ctx_t *, void *);

typedef struct handler_s {
    int fd;
    void *data;
    fptr_t callback;
} handler_t;

#endif /* COORDINATOR_H_ */
