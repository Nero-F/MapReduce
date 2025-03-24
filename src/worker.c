#include "common.h"
#include "worker.h"
#include "coordinator.h"
#include <sys/stat.h>
#include "frpc.h"
#include <bits/getopt_core.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>

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
    if (worker->sockfd != -1) close(worker->sockfd);
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
    int sockfd = worker->sockfd;
    struct addrinfo *coord_info = worker->coord_info;
    uint try_nbr = 0;

    request_t req = {
        .ack = ACK,
        .id = id++,
        .op = op,
    };

try_call:
    if (sendto(sockfd, &req, sizeof(request_t), 0, coord_info->ai_addr,
            coord_info->ai_addrlen)
        == -1)
        RETRY_CALL("sendto");

    if (recvfrom(sockfd, resp, sizeof(response_t), 0, coord_info->ai_addr,
            &coord_info->ai_addrlen)
        == -1)
        RETRY_CALL("recvfrom");

    return SUCCESS;
}

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

void do_task(work_t task_work, worker_t *worker)
{
    if (task_work.type == MAP) _map(task_work.id, task_work.split, worker->map);
}

int work(worker_t *worker)
{
    response_t resp = { 0 };

    CALL(REQ_NREDUCE, worker, &resp, 2);
    worker->n_reduce = resp.data.nrduce;

    while (1) {
        CALL(REQ_WORK, worker, &resp, 0);
        if (resp.data.task_work.type != NONE) break;
        sleep(30);
    }
    do_task(resp.data.task_work, worker);
    return SUCCESS;
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
    worker->sockfd = sockfd;
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

    // TODO: Periodically ask for work

    return SUCCESS;
}

int main(const int argc, char *const argv[])
{
    worker_t worker = {
        .plug_path = "./plug.so",
        .coord_addr = DEFAULT_COORD_ADDR,
        .coord_port = DEFAULT_COORD_PORT,
    };

    if (parse_arg(argc, argv, &worker) == FAILURE
        || load_mr_plugs(worker.plug_path, &worker) == FAILURE
        || connect_to_coord(&worker) == FAILURE)
        return FAILURE;
    work(&worker);
    end_coord(&worker, NULL);
    return SUCCESS;
}
