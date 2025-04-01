#include <errno.h>
#include "frpc.h"
#include "coordinator.h"

response_t ask_work(asked_t asked, coordinator_t *coord)
{
    response_t res = {
        .id = asked.req.id,
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
        if ((*task).worker.state == IDLE) {
            (*task).worker.state = IN_PROGESS;
            (*task).worker.id = asked.cli_fd;
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

response_t ask_nreduce(asked_t asked, coordinator_t *coord)
{
    printf("requesting nreduce...\n");

    response_t resp = {
        .id = asked.req.id,
        .op = asked.req.op,
        .data = (inner_data_u) {
            .nrduce = coord->n_reduce,
        },
    };
    return resp;
}

response_t (*fptr_tbl[])(asked_t asked, coordinator_t *coord) = {
    &ask_work,
    &ask_nreduce,
    NULL,
};

typedef struct pinger_data_s {
    work_t work;
    int cli_fd;
} pinger_data_t;

// This is our way to check if coordinator is still reachable
void *work_pinger(void *data)
{
    printf("start work_pinger...\n");
    pinger_data_t *p_data = (pinger_data_t *)data;
    struct timeval timeout = { .tv_sec = 10 };
    printf("cli_fd -> %d\n", p_data->cli_fd);
    if (setsockopt(p_data->cli_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
            sizeof(struct timeval))
        == -1) {
        perror("setsockopt");
        return NULL;
    }

    request_t req = {
        .op = PING,
        .id = -1,
    };
    msg_t msg = {
        .ack = ACK,
        .type = REQUEST,
        .data.req = req,
    };
    while (1) {
        printf("sending heartbeat to [WORKER:%d]\n", p_data->cli_fd);
        if (send(p_data->cli_fd, &msg, sizeof(msg_t), 0) == -1) {
            if (errno == ETIMEDOUT) fprintf(stderr, "timedout on send");
            perror("send HEARTBEAT");
            return NULL;
        }
        sleep(10);
        // Epoll will recv the response so check there if its ok with timelimit
    }

    return NULL;
}

int start_ping_thread(pthread_t *thread, int cli_fd, work_t task_work)
{
    pinger_data_t *p_data = malloc(sizeof(pinger_data_t));
    pthread_t thrd = { 0 };

    ASSERT_MEM_CTX(p_data, "ping thread");
    p_data->cli_fd = cli_fd;
    p_data->work = task_work;

    printf("thread  >>>>> %p\n", thread);

    if (pthread_create(&thrd, NULL, &work_pinger, p_data) != 0) {
        perror("Failed init ping thread");
        return FAILURE;
    }
    thread = &thrd;
    return SUCCESS;
}
int process_req(int cli_fd, request_t req, coordinator_t *coord)
{
    response_t resp = fptr_tbl[req.op]((asked_t) { req, cli_fd }, coord);
    msg_t msg = {
        .ack = ACK,
        .type = RESPONSE,
        .data.res = resp,
    };

    if (send(cli_fd, &msg, sizeof(msg_t), 0) == -1) {
        perror("send");
        return FAILURE;
    }
    // If work has been assigned we periodically ping workers to check failed
    // workers (cf. 3.3 Fault tolerance)
    foreach_ll(task_t, t, coord->map_task)
    {
        printf("@ [TASK:%d][machine:%d|state:%d]", t->id, t->worker.id,
            t->worker.state);
        printf(" thread running: %s \n", t->thread ? "true" : "false");
        if (t->thread == NULL && resp.op == REQ_WORK
            && resp.data.task_work.type != NONE)
            return start_ping_thread(t->thread, cli_fd, resp.data.task_work);
    }
    return SUCCESS;
}
