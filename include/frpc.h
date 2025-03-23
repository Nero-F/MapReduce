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
} __attribute__((packed)) request_t;

typedef struct work_s {
    task_type_t type;
    char *split;
    int id;
} work_t;

typedef union inner_data {
    int nrduce;
    work_t task_work;
} inner_data_u;

typedef struct response_s {
    inner_data_u data;
    opcode_t op;
    byte ack;
    byte id;
} __attribute__((packed)) response_t;

int process_req(int, request_t, coordinator_t *);

#endif /* _F_RPC_H_ */
