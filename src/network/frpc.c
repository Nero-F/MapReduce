#include <stdio.h>
#include "frpc.h"
#include "coordinator.h"

// response_t ask_nreduce
response_t ask_work(request_t req, coordinator_t *coord)
{
    response_t res = {
        .id = req.id,
        .ack = ACK,
        .op = REQ_WORK,
    };

    work_t work = { 0 };
    llist_t tasks = NULL;

    // Check the length of the number of task
    if (coord->n_map > 0) {
        work.id = coord->n_map;
        work.type = MAP;
        tasks = coord->map_task;
    } else if (coord->n_reduce > 0) {
        work.id = coord->n_reduce;
        work.type = REDUCE;
        tasks = coord->reduce_task;
    } else {
        work.type = NONE;
        return res;
    }

    node_t *node = tasks;
    while (node) {
        task_t *task = (task_t *)node->value;
        printf("[%d] :: task[%d], state: %d\n", task->type, task->id,
            task->worker.state);
        if ((*task).worker.state == IDLE) {
            (*task).worker.state = IN_PROGESS;
            (*task).type = work.type;
            work.split = coord->files->items[task->id];
            res.data.task_work = work;
            break;
        }
        node = node->next;
    }
    printf("[%d] task %d was assigned to machine with split {%s}\n",
        res.data.task_work.type, res.data.task_work.id,
        res.data.task_work.split);
    return res;
}
response_t ask_nreduce(request_t req, coordinator_t *coord)
{
    printf("requesting nreduce...\n");

    response_t resp = {
        .ack = ACK,
        .id = req.id,
        .op = req.op,
        .data = (inner_data_u) {
            .nrduce = coord->n_reduce,
        },
    };
    return resp;
}

response_t (*fptr_tbl[])(request_t req, coordinator_t *coord) = {
    &ask_work,
    &ask_nreduce,
    NULL,
};

int process_req(int cli_fd, request_t req, coordinator_t *coord)
{
    response_t resp = fptr_tbl[req.op](req, coord);
    if (send(cli_fd, &resp, sizeof(response_t), 0) == -1) {
        perror("send");
        return FAILURE;
    }
    return SUCCESS;
}
