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
    return (response_t) {
        .ack = ACK,
        .id = req.id,
        .op = req.op,
        .data = (void *)&coord->n_reduce,
    };
}

response_t (*fptr_tbl[])(request_t req, coordinator_t *coord) = {
    &ask_work,
    &ask_nreduce,
    NULL,
};

response_t process_req(request_t req, coordinator_t *coord)
{
    return fptr_tbl[req.op](req, coord);
}
