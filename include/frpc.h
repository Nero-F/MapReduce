#ifndef _F_RPC_H_
#define _F_RPC_H_

#include "coordinator.h"

#define ACK 0xF

typedef enum opcode_e {
    REQ_WORK = 0,
    REQ_NREDUCE,
    TASK_DONE,
} opcode_t;

typedef char byte;
typedef struct request_s {
    opcode_t op;
    byte ack;
    byte id;
} request_t;

typedef struct response_s {
    opcode_t op;
    void *data;
    byte ack;
    byte id;
} response_t;

response_t process_req(request_t, coordinator_t *);

#endif /* _F_RPC_H_ */
