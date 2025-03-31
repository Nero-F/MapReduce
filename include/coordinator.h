#ifndef COORDINATOR_H_
#define COORDINATOR_H_

#include "common.h"
#include "mapreduce.h"
#include "linked_list.h"

#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_RUNNING_PORT 4269
#define DEFAULT_NREDUCE 3

#define OUT_PREFIX "mr-out-"

typedef struct machine_s {
    task_state_t state;
    int id;
} machine_t;

typedef struct task_s {
    task_type_t type;
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

    bool pinger_running;
    pthread_rwlock_t rwlock;
} coordinator_t;

// Network stuffs

#define MAX_EVENTS 42

typedef struct tcp_sock_s {
    struct sockaddr_in addr;
    int fd;
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

typedef int (*fptr_t)(network_ctx_t *, void *, coordinator_t *);

typedef struct handler_s {
    int fd;
    void *data;
    fptr_t callback;
} handler_t;

int run_server(coordinator_t coord);

#endif /* COORDINATOR_H_ */
