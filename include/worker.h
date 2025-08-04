#ifndef WORKER_H_
#define WORKER_H_

#include "frpc.h"
#include <assert.h>
#include <dlfcn.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_COORD_ADDR "localhost"
#define DEFAULT_COORD_PORT "4269"

#define PLUGS_MR_LIST                                                          \
  PLUG(map, kva_t, const char *, const char *)                                 \
  PLUG(reduce, char *, const char *, const char **)

#define PLUG(func_name, ret_val, ...)                                          \
  typedef ret_val(func_name##_t)(__VA_ARGS__);
PLUGS_MR_LIST
#undef PLUG

#define CALL(op, worker, resp, try_nbr)                                        \
  do {                                                                         \
    if (call(op, worker, resp, try_nbr) == FAILURE) {                          \
      fprintf(stderr, "could not contact coordinator\n");                      \
      return FAILURE;                                                          \
    }                                                                          \
  } while (0);

#define SEND(op, worker, data, try_nbr)                                        \
  do {                                                                         \
    if (_send(op, worker, data, try_nbr) == FAILURE) {                         \
      fprintf(stderr, "could not contact coordinator\n");                      \
      return FAILURE;                                                          \
    }                                                                          \
                                                                               \
  } while (0);

// task_state_t state;
typedef struct worker_s {
  void *plug_handle;
  char *plug_path;
  char *coord_addr;
  char *coord_port;

  map_t *map;
  reduce_t *reduce;

  // coord socket file descriptor
  int coord_fd;

  int n_reduce;
  task_state_t state;
  struct addrinfo *coord_info;

  pthread_rwlock_t rwlock;
} worker_t;

int load_mr_plugs(const char *, worker_t *);
int connect_to_coord(worker_t *worker);
int end_coord(worker_t *worker, char *err_msg);
int call(opcode_t op, worker_t *worker, response_t *resp, uint retry);

#endif /* WORKER_H_ */
