#include <stdio.h>
#include "frpc.h"
#include "coordinator.h"

// response_t ask_nreduce
response_t ask_work(request_t __attribute__((unused)) req,
    coordinator_t __attribute__((unused)) * coord)
{
    response_t res = { 0 };
    printf("requesting operations...\n");
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
