#ifndef WORKER_H_
#define WORKER_H_

#include <assert.h>
#include <stddef.h>
#include <dlfcn.h>
#include <getopt.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define DEFAULT_COORD_ADDR "localhost"
#define DEFAULT_COORD_PORT "4269"

#define PLUGS_MR_LIST                                                          \
    PLUG(map, int, const char *, const char *)                                 \
    PLUG(reduce, int, int, const char *, const char *)

#define PLUG(func_name, ret_val, ...)                                          \
    typedef ret_val(func_name##_t)(__VA_ARGS__);
PLUGS_MR_LIST
#undef PLUG

typedef struct worker_s {
    void *plug_handle;
    char *plug_path;
    char *coord_addr;
    char *coord_port;

    map_t *map;
    reduce_t *reduce;

    int sockfd;

    int n_reduce;
    struct addrinfo *coord_info;
} worker_t;

int load_mr_plugs(const char *, worker_t *);

#endif /* WORKER_H_ */
