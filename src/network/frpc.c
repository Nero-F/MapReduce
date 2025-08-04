#include <errno.h>
#include "defer.h"
#include "frpc.h"
#include "common.h"
#include "coordinator.h"
#include "mapreduce.h"

payload_t ask_work(asked_t asked, coordinator_t *coord)
{
    pthread_mutex_lock(&coord->mu);
    DEFER({ pthread_mutex_unlock(&coord->mu); });
    payload_t resp = {
        .id = asked.req.id,
        .op = REQ_WORK,
    };

    work_t work = { 0 };
    llist_t tasks = NULL;

    if (coord->n_map > 0) {
        work.type = MAP;
        tasks = coord->map_task;
    } else if (coord->n_reduce > 0) {
        work.type = REDUCE;
        tasks = coord->reduce_task;
    } else {
        work.type = NONE;
        return resp;
    }

    node_t *node = tasks;
    int id = 0;
    while (node) {
        task_t *task = (task_t *)node->value;
        if ((*task).worker.state == IDLE) {
            (*task).worker.state = IN_PROGRESS;
            (*task).worker.id = asked.cli_fd;
            (*task).type = work.type;
            strcpy(work.split, coord->files->items[task->id]);
            work.id = id;
            resp.data.task_work = work;
            break;
        }
        ++id;
        node = node->next;
    }
    printf("[%d] task %d was assigned to machine with split {%s}\n",
        resp.data.task_work.type, resp.data.task_work.id,
        resp.data.task_work.split);
    return resp;
}

payload_t ask_nreduce(asked_t asked, coordinator_t *coord)
{
    printf("requesting nreduce...\n");

    return (payload_t) {
        .id = asked.req.id,
        .op = asked.req.op,
        .data = (inner_data_u) {
            .nrduce = coord->n_reduce,
        },
    };
}

bool end_task(llist_t tasks, int id)
{
    task_t *task = list_get_elem_at_position(tasks, id);
    ASSERT_MEM_CTX(task, "Did not succeed to change task's state\n");
    printf(" ^^^thread running: %s \n", task->thread != 0 ? "true" : "false");

    // // pthread_rwlock_wrlock(&rwlock);
    // task->worker.state = COMPLETED;
    // // pthread_rwlock_unlock(&rwlock);
    //
    // if (task->thread != 0 && pthread_join(task->thread, NULL) != 0) {
    //     fprintf(stderr, "Did not succeed to join thread properly...\n");
    //     return false;
    // }
    task->thread = 0;
    return true;
}

payload_t task_done(asked_t asked, coordinator_t *coord)
{
    pthread_mutex_lock(&coord->mu);
    DEFER({ pthread_mutex_unlock(&coord->mu); });
    printf("[WORKER:%d][TASK:%d] done +++++++++++=.\n", asked.cli_fd,
        asked.req.data.task_work.id);
    work_t task_work = asked.req.data.task_work;

    // TBD: maybe use a thread lock
    switch (task_work.type) {
        case MAP:
            coord->n_map -= 1;
            end_task(coord->map_task, task_work.id);
            break;
        case REDUCE:
            coord->n_reduce -= 1;
            end_task(coord->reduce_task, task_work.id);
            break;
        default:
            break;
    }
    return (payload_t) {
        .id = asked.req.id,
        .op = asked.req.op,
    };
}

payload_t (*fptr_tbl[])(asked_t asked, coordinator_t *coord) = {
    &ask_work,
    &ask_nreduce,
    &task_done,
    NULL,
};

typedef struct pinger_data_s {
    work_t work;
    // task_state_t *state;
    int cli_fd;
    bool done;
} pinger_data_t;

// This is our way to check if worker is still reachable
void *worker_pinger(void *data)
{
    printf("start work_pinger...\n");
    pinger_data_t *p_data = (pinger_data_t *)data;
    struct timeval timeout = { .tv_sec = 10 };

    if (setsockopt(p_data->cli_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
            sizeof(struct timeval))
        == -1) {
        perror("setsockopt");
        return NULL;
    }
    msg_t msg = { .ack = ACK,
        .type = REQUEST,
        .payload = (payload_t) {
            .op = PING,
            .id = -1,
        },
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

int start_ping_thread(
    pthread_t *thread, task_state_t *state, int cli_fd, work_t task_work)
{
    pinger_data_t *p_data = malloc(sizeof(pinger_data_t));

    ASSERT_MEM_CTX(p_data, "ping thread");
    p_data->cli_fd = cli_fd;
    p_data->work = task_work;
    // p_data->state = state;

    if (pthread_create(thread, NULL, &worker_pinger, p_data) != 0) {
        perror("Failed init ping thread");
        return FAILURE;
    }
    return SUCCESS;
}

int process_req(int cli_fd, payload_t req, coordinator_t *coord)
{
    payload_t resp = fptr_tbl[req.op]((asked_t) { req, cli_fd }, coord);
    msg_t msg = {
        .ack = ACK,
        .type = RESPONSE,
        .payload = resp,
    };

    if (send(cli_fd, &msg, sizeof(msg_t), 0) == -1) {
        perror("send");
        return FAILURE;
    }
    if (resp.op == REQ_NREDUCE) return SUCCESS;

    // If work has been assigned we periodically ping workers to check failed
    // workers (cf. 3.3 Fault tolerance)
    foreach_ll(task_t, t, coord->map_task)
    {
        printf("@ [TASK:%d][machine:%d|state:%d]", t->id, t->worker.id,
            t->worker.state);
        printf(" thread running: %s \n", t->thread != 0 ? "true" : "false");
        if (resp.op == REQ_WORK && resp.data.task_work.type != NONE
            && t->thread == 0)
            return start_ping_thread(
                &t->thread, &t->worker.state, cli_fd, resp.data.task_work);
    }
    // TBD: end ping thread after task work done so that reduce reinit other
    // pinger threads
    foreach_ll(task_t, t, coord->reduce_task)
    {
        printf("@ [TASK:%d][machine:%d|state:%d]", t->id, t->worker.id,
            t->worker.state);
        printf(" thread running: %s \n", t->thread != 0 ? "true" : "false");
        if (resp.op == REQ_WORK && resp.data.task_work.type != NONE
            && t->thread == 0)
            return start_ping_thread(
                &t->thread, &t->worker.state, cli_fd, resp.data.task_work);
    }
    return SUCCESS;
}
