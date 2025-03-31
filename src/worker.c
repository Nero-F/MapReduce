#include "common.h"
#include "worker.h"
#include "coordinator.h"
#include <sys/stat.h>
#include "frpc.h"
#include <bits/getopt_core.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static void display_help(void)
{
    printf("Usage: worker [options(s)]\n");
    printf("\t-H, --host\thost address of the coordinator, (default: %s)\n",
        DEFAULT_COORD_ADDR);
    printf("\t-P, --port\thost port of the coordinator, (default: %s)\n",
        DEFAULT_COORD_PORT);
    printf("\t-p, --plugin\tpath of the plugin that need to be executed\n");
    printf("\t-h, --help\tdisplay this help list\n");
}

static int parse_arg(const int ac, char *const av[], worker_t *worker)
{
    static struct option opts[] = {
        { "host", required_argument, NULL, 'H' },
        { "port", required_argument, NULL, 'P' },
        { "plugin", required_argument, NULL, 'p' },
        { "help", no_argument, NULL, 'h' },
    };
    int opt_idx = 0;
    char ret_opt = 0;

    while ((ret_opt = getopt_long(ac, av, "H:P:p:", opts, &opt_idx)) != -1) {
        switch (ret_opt) {
            case 'H':
                worker->coord_addr = optarg;
                break;
            case 'P':
                worker->coord_port = optarg;
                break;
            case 'p':
                worker->plug_path = optarg;
                break;
            case 'h':
                display_help();
                break;
            default:
                return FAILURE;
        }
    }
    return SUCCESS;
}

static int end_coord(worker_t *worker, char *err_msg)
{
    if (err_msg != NULL) perror(err_msg);
    if (worker->plug_handle) dlclose(worker->plug_handle);
    if (worker->coord_fd != -1) close(worker->coord_fd);
    if (worker->coord_info != NULL) freeaddrinfo(worker->coord_info);
    return FAILURE;
}

#define RETRY_CALL(err_msg)                                                    \
    do {                                                                       \
        if (try_nbr < retry) {                                                 \
            try_nbr++;                                                         \
            goto try_call;                                                     \
        } else {                                                               \
            perror(err_msg);                                                   \
            return FAILURE;                                                    \
        }                                                                      \
    } while (0)

int call(opcode_t op, worker_t *worker, response_t *resp, uint retry)
{
    static size_t id = 0;
    int sockfd = worker->coord_fd;
    struct addrinfo *coord_info = worker->coord_info;
    uint try_nbr = 0;

    msg_t msg = {
        .ack = ACK,
        .type = REQUEST,
        .data.req = {
        .id = id++,
        .op = op,
        },
    };

try_call:
    if (sendto(sockfd, &msg, sizeof(msg_t), 0, coord_info->ai_addr,
            coord_info->ai_addrlen)
        == -1)
        RETRY_CALL("sendto");

    if (recvfrom(sockfd, &msg, sizeof(msg_t), 0, coord_info->ai_addr,
            &coord_info->ai_addrlen)
        == -1)
        RETRY_CALL("recvfrom");
    *resp = msg.data.res;
    return SUCCESS;
}

// TODO: maybe check resp ACK
#define CALL(op, worker, resp, try_nbr)                                        \
    do {                                                                       \
        if (call(op, worker, resp, 0) == FAILURE) {                            \
            fprintf(stderr, "could not contact coordinator\n");                \
            return FAILURE;                                                    \
        }                                                                      \
    } while (0)

bool _map(const uint id, const char *filename, map_t *emit)
{
    int fd = open(filename, O_RDONLY);
    struct stat statbuf = { 0 };
    int file_size = 0;
    char *buffer = NULL;

    if (fd == -1) {
        perror("fopen");
        return false;
    }
    if (fstat(fd, &statbuf) == -1) {
        close(fd);
        perror("fopen");
        return false;
    }
    file_size = statbuf.st_size;
    buffer = malloc(sizeof(char) * (file_size + 1));
    if (buffer == NULL) {
        close(fd);
        return false;
    }

    buffer[file_size] = '\0';
    read(fd, buffer, file_size);

    emit(filename, buffer);

    free(buffer);
    close(fd);
    return true;
}

typedef struct thread_work_s {
    worker_t *worker;
    work_t work;
} thread_work_t;

void *do_task(void *data)
{
    thread_work_t *twork = (thread_work_t *)data;
    worker_t *worker = twork->worker;

    // if (twork->work.type == MAP)
    // _map(twork->work.id, twork->work.split, twork->worker->map);

    printf("dowork\n");
    while (1) {
    }
    // else if (twork->work.type == REDUCE)
    //     _reduce(twork->work.id, twork->work.split, twork->worker->map);

    return NULL;
}

// TODO: better logs + determine if exiting here
// will recv ping request
int ping_handler(worker_t *worker)
{
    int ret_recv = 0;
    msg_t msg_req = { 0 };

    while (1) {
        pthread_rwlock_rdlock(&worker->rwlock);
        if (worker->state == COMPLETED) break;
        pthread_rwlock_unlock(&worker->rwlock);
        if ((ret_recv = recvfrom(worker->coord_fd, &msg_req, sizeof(msg_t), 0,
                 worker->coord_info->ai_addr, &worker->coord_info->ai_addrlen))
            == -1) {
            perror("recv ping");
        } else if (ret_recv == 0) {
            printf("[[COORDINATOR EXIT]]...\n");
            break;
        }
        if (msg_req.ack != ACK) {
            fprintf(stderr,
                "could not recognize following request with ID: %d from %s\n",
                msg_req.data.req.id, worker->coord_addr);
            continue;
        }
        assert(msg_req.data.req.op == PING);
        printf("<<PING>> received\n");
        msg_t msg_resp = {
            .ack = ACK,
            .type = RESPONSE,
            .data.req = {
                .id = msg_req.data.req.id,
                .op = msg_req.data.req.op,
            },
        };
        if (sendto(worker->coord_fd, &msg_resp, sizeof(msg_t), 0,
                worker->coord_info->ai_addr, worker->coord_info->ai_addrlen)
            == -1) {
            perror("sendto ping");
        }
    }
    return SUCCESS;
}

pthread_t *exec_task_thread(worker_t *worker, work_t task_work)
{
    pthread_t *thread = malloc(sizeof(pthread_t));
    ASSERT_MEM_CTX(thread, "Task thread");

    thread_work_t twork = {
        .worker = worker,
        .work = task_work,
    };
    if (pthread_create(thread, NULL, &do_task, &twork) != 0) {
        perror("pthread_create");
        return NULL;
    }
    return thread;
}

int work(worker_t *worker)
{
    response_t resp = { 0 };
    int ret_val = SUCCESS;

    printf("<<REQ_NREDUCE>>...\n");
    CALL(REQ_NREDUCE, worker, &resp, 2);
    worker->n_reduce = resp.data.nrduce;
    printf("<<REQ_NREDUCE SUCCESS>> value: %d\n", resp.data.nrduce);

    while (1) {
        printf("<<REQ_WORK>>...\n");
        CALL(REQ_WORK, worker, &resp, 0);
        if (resp.data.task_work.type != NONE) break;
        sleep(15);
    }
    printf("<<REQ_WORK SUCCESS>> type: %d\n", resp.data.task_work.type);
    pthread_t *thread = exec_task_thread(worker, resp.data.task_work);
    if (thread == NULL) return FAILURE;
    ret_val = ping_handler(worker);
    pthread_detach(*thread);
    free(thread);
    return ret_val;
}

static int connect_to_coord(worker_t *worker)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int ret_gai = 0;
    struct sockaddr_in sockaddr = { 0 };

    struct addrinfo *coord_info = { 0 };

    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET; // TBD: maybe change for AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // whildcard IP address
    hints.ai_protocol = IPPROTO_TCP;

    if (sockfd == -1) return end_coord(worker, "socket");
    worker->coord_fd = sockfd;
    if (bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1)
        return end_coord(worker, "bind");

    ret_gai = getaddrinfo(
        worker->coord_addr, worker->coord_port, &hints, &coord_info);

    if (ret_gai != 0) {
        fprintf(stderr, "[ERROR]: %s\n", gai_strerror(ret_gai));
        return end_coord(worker, NULL);
    }
    worker->coord_info = coord_info;
    if (connect(sockfd, coord_info->ai_addr, coord_info->ai_addrlen) == -1)
        return end_coord(worker, "connect");

    return SUCCESS;
}

int main(const int argc, char *const argv[])
{
    worker_t worker = {
        .plug_path = "./plug.so",
        .coord_addr = DEFAULT_COORD_ADDR,
        .coord_port = DEFAULT_COORD_PORT,
        .state = IDLE,
    };

    if (parse_arg(argc, argv, &worker) == FAILURE
        || load_mr_plugs(worker.plug_path, &worker) == FAILURE
        || connect_to_coord(&worker) == FAILURE)
        return FAILURE;
    work(&worker);
    end_coord(&worker, NULL);
    return SUCCESS;
}
