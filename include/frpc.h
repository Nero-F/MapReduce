#ifndef _F_RPC_H_
#define _F_RPC_H_

#include "coordinator.h"
#include <stddef.h>

#define MAX_FILENAME_LENGTH 256
#define ACK 0xF

typedef enum opcode_e {
  REQ_WORK = 0,
  REQ_NREDUCE,
  PING, // from coord to worker only
  TASK_DONE,
} opcode_t;

typedef char byte;
typedef struct request_s {
  opcode_t op;
  byte id;
} __attribute__((packed)) request_t;

typedef struct work_s {
  char split[MAX_FILENAME_LENGTH];
  task_type_t type;
  int id;
} __attribute__((packed)) work_t;

typedef union inner_data {
  work_t task_work;
  char intermediate_result[MAX_FILENAME_LENGTH];
  int nrduce;
} __attribute__((packed)) inner_data_u;

typedef struct response_s {
  inner_data_u data;
  opcode_t op;
  byte id;
} __attribute__((packed)) response_t;

typedef union msg_data {
  request_t req;
  response_t res;
} msg_data_u;

typedef enum msg_type_e {
  REQUEST = 0,
  RESPONSE,
} msg_type_t;

typedef struct msg_s {
  msg_type_t type;
  msg_data_u data;
  byte ack;
} __attribute__((packed)) msg_t;

typedef struct asked_s {
  request_t req;
  int cli_fd;
} asked_t;

int process_req(int, request_t, coordinator_t *);

#endif /* _F_RPC_H_ */
