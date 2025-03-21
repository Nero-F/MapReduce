#ifndef COORDINATOR_H_
#define COORDINATOR_H_

#include "common.h"
#include "linked_list.h"

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

#define DEFAULT_RUNNING_PORT 4269
#define DEFAULT_NREDUCE 3

typedef enum task_state_e {
    IDLE = 0,
    IN_PROGESS,
    COMPLETED,
} task_state_t;

typedef enum task_type_e {
    NONE = 0,
    MAP,
    REDUCE,
} task_type_t;

typedef struct machine_s {
    task_state_t state;
    int id;
} machine_t;

typedef struct task_s {
    task_type_t state;
    machine_t worker;
    int id;
} task_t;

typedef struct coordinator_s {
    int n_map;
    int n_reduce;

    llist_t map_task;
    llist_t reduce_task;

    files_t *files; // input file splits
    files_t output_files;

    int running_port;
} coordinator_t;

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

int run_server(coordinator_t coord);

#endif /* COORDINATOR_H_ */
