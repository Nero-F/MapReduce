#include "common.h"
#include "worker.h"
#include "defer.h"
#include "frpc.h"
#include "mapreduce.h"

#include <sys/cdefs.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <bits/getopt_core.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

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

int end_coord(worker_t *worker, char *err_msg)
{
    if (err_msg != NULL) perror(err_msg);
    if (worker->plug_handle) dlclose(worker->plug_handle);
    if (worker->coord_fd != -1) close(worker->coord_fd);
    if (worker->coord_info != NULL) freeaddrinfo(worker->coord_info);
    if (worker->work_thread != NULL) free(worker->work_thread);
    pthread_mutex_destroy(&worker->mu);
    return FAILURE;
}

int encode_to_file(FILE *fs, kva_t *kva)
{
    foreach (kv_t, kv, kva) {
        int len_key = strlen(kv->key);
        int len_value = strlen(kv->value);
        fwrite(&len_key, sizeof(int), 1, fs);
        fwrite(kv->key, sizeof(char), len_key, fs);
        fwrite(&len_value, sizeof(int), 1, fs);
        fwrite(kv->value, sizeof(char), len_value, fs);
    }
    return SUCCESS;
}

int write_result(kva_t *intermediate, int id, int reduce_id)
{
    char *tmp_filename = NULL;

    asprintf(&tmp_filename, "mr-%d-%d-XXXXXX", id, reduce_id);
    ASSERT_MEM_CTX(tmp_filename, "Intermediate result writing");
    DEFER({ free(tmp_filename); });
    int tmp_fd = mkstemp(tmp_filename);

    if (tmp_fd == -1) {
        perror("Intermediate tmp result file creation");
        return FAILURE;
    }
    DEFER({ close(tmp_fd); });
    FILE *tmp_fs = fdopen(tmp_fd, "w");
    if (tmp_fs == NULL) {
        perror("Intermediate tmp result file opening");
        unlink(tmp_filename);
        return FAILURE;
    }
    DEFER({ fclose(tmp_fs); });
    if (encode_to_file(tmp_fs, intermediate) == FAILURE) {
        perror("Intermediate tmp result encoding");
        unlink(tmp_filename);
        return FAILURE;
    }

    char *oname = NULL;
    asprintf(&oname, "mr-%d-%d", id, reduce_id);
    ASSERT_MEM_CTX(oname, "Intermediate result writing");
    DEFER({ free(oname); });
    if (rename(tmp_filename, oname) == -1) {
        perror("Intermediate tmp file renaming");
        unlink(tmp_filename);
        return FAILURE;
    }
    printf("Successfully saved result to %s.\n", oname);
    return SUCCESS;
}

#define FNV_PRIME 0x100000001b3
#define FNV_OFFSET_BASIS 0xcbf29ce484222325
uint64_t hash(const char *key)
{
    uint64_t h = FNV_OFFSET_BASIS;
    size_t key_len = strlen(key);
    size_t i = 0;

    while (i < key_len) {
        h ^= key[i++];
        h *= FNV_PRIME;
    }
    return h;
}

int save_intermediate_result(const uint id, kva_t kva, const uint n_reduce)
{
    kva_t *intermediate = malloc(sizeof(kva_t) * (n_reduce + 1));

    ASSERT_MEM_CTX(intermediate, "Intermediate result writing");
    DEFER({ free(intermediate); });

    for (uint i = 0; i < n_reduce; i++) {
        memset(&intermediate[i], 0, sizeof(kva_t));
    }
    foreach (kv_t, kv, &kva) {
        int r = hash(kv->key) % n_reduce;
        da_append(&intermediate[r], *kv);
    }

    for (uint idx = 0; idx < n_reduce; ++idx) {
        if (write_result(intermediate, id, idx) == FAILURE) return FAILURE;
    }
    return SUCCESS;
}

bool _map(const uint id, const char *filename, map_t *emit, const uint n_reduce)
{
    int res = SUCCESS;
    int fd = open(filename, O_RDONLY);
    struct stat statbuf = { 0 };
    int file_size = 0;
    char *buffer = NULL;

    if (fd == -1) {
        perror("open");
        return false;
    }
    DEFER({ close(fd); });
    if (fstat(fd, &statbuf) == -1) {
        perror("fstat");
        return false;
    }
    file_size = statbuf.st_size;
    buffer = malloc(sizeof(char) * (file_size + 1));
    ASSERT_MEM(buffer);
    DEFER({ free(buffer); });

    buffer[file_size] = '\0';
    read(fd, buffer, file_size);
    kva_t kva = emit(filename, buffer);
    res = save_intermediate_result(id, kva, n_reduce);

    return res;
}

typedef struct thread_work_s {
    worker_t *worker;
    work_t work;
} thread_work_t;

int end_task(thread_work_t *twork, payload_t *resp)
{
    resp->data.task_work
        = twork->work; // just a little hack to pass in the work type to msg
    SEND(TASK_DONE, twork->worker, resp->data, 3);

    LOCK(&twork->worker->mu);
    twork->worker->state = IDLE;
    UNLOCK(&twork->worker->mu);
    return SUCCESS;
}

void *do_task(void *data)
{
    thread_work_t *twork = (thread_work_t *)data;
    payload_t resp = { 0 };

    if (twork->work.type == MAP
        && _map(twork->work.id, twork->work.split, twork->worker->map,
               twork->worker->n_reduce)
            == SUCCESS) {
        end_task(twork, &resp);
    }

    return SUCCESS;
}

void check_ping(worker_t *worker, payload_t req)
{
    msg_t ping_resp = {
            .ack = ACK,
            .type = RESPONSE,
            .payload = {
                .id = req.id,
                .op = req.op,
            },
    };
    if (sendto(worker->coord_fd, &ping_resp, sizeof(msg_t), 0,
            worker->coord_info->ai_addr, worker->coord_info->ai_addrlen)
        == -1) {
        perror("sendto ping");
    }
}

int exec_task_thread(worker_t *worker, work_t task_work)
{
    pthread_t *thread = malloc(sizeof(pthread_t));
    ASSERT_MEM_CTX(thread, "Task thread");

    thread_work_t twork = {
        .worker = worker,
        .work = task_work,
    };
    if (pthread_create(thread, NULL, &do_task, &twork) != 0) {
        perror("pthread_create");
        free(thread);
        return FAILURE;
    }
    worker->work_thread = thread;
    return SUCCESS;
}

int req_nreduce(worker_t *worker)
{
    payload_t resp = { 0 };
    printf("<<REQ_NREDUCE>>...\n");
    CALL(REQ_NREDUCE, worker, &resp, 2);
    worker->n_reduce = resp.data.nrduce;
    printf("<<REQ_NREDUCE SUCCESS>> value: %d\n", resp.data.nrduce);
    return SUCCESS;
}

int req_work(worker_t *worker)
{
    printf("[EVENT]: req work..\n");
    inner_data_u data = { 0 };
    SEND(REQ_WORK, worker, data, 2);
    return SUCCESS;
}

// TODO: on error close fds
int init_epoll(worker_t *worker)
{
    struct epoll_event ev;
    struct itimerspec timer_spec
        = { .it_interval = { 10, 0 }, .it_value = { 1, 0 } };
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (timerfd == -1) {
        perror("timerfd_create");
        return FAILURE;
    }
    if (timerfd_settime(timerfd, 0, &timer_spec, NULL) == -1) {
        perror("timerfd_settime");
        return FAILURE;
    }
    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd == -1) {
        perror("epoll_create1");
        return FAILURE;
    }
    ev.events = EPOLLIN;
    ev.data.fd = timerfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev);
    ev.data.fd = worker->coord_fd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, worker->coord_fd, &ev);

    int evfd = eventfd(0, EFD_NONBLOCK);
    if (evfd == -1) {
        perror("eventfd");
        return FAILURE;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, evfd, &ev);

    worker->evfd = evfd;
    worker->epollfd = epollfd;
    worker->timerfd = timerfd;
    return SUCCESS;
}

int handle_ping(void)
{
    printf("[EVENT]: supposed to respond to pong\n");
    return SUCCESS;
}

int handle_work(worker_t *worker, payload_t payload)
{
    printf("[EVENT]: supposed start work\n");

    LOCK(&worker->mu);
    worker->state = IN_PROGRESS;
    UNLOCK(&worker->mu);
    printf("[WORKER DATA]: %s\n", payload.data.task_work.split);

    return exec_task_thread(worker, payload.data.task_work);
}

int event_handler(worker_t *worker)
{
    payload_t payload = { 0 };
    int r = _recv(worker, &payload);
    if (r != SUCCESS) return r;

    switch (payload.op) {
        case PING:
            return handle_ping();
        case REQ_WORK:
            return handle_work(worker, payload);
        case TASK_DONE:
            printf("<<REQ_WORK SUCCESS>>\n");
            // sending event to epoll timerfd
            struct itimerspec ntimer_spec
                = { .it_interval = { 10, 0 }, .it_value = { 1, 0 } };
            timerfd_settime(worker->timerfd, 0, &ntimer_spec, NULL);
            break;
        default:
            printf("[EVENT]: supposed to respond to shit\n");
    }
    return SUCCESS;
}

int work(worker_t *worker)
{
    int ret = SUCCESS;
    struct epoll_event events[MAX_EVENTS];
    int nfds = 0;

    if (req_nreduce(worker) != SUCCESS || init_epoll(worker) != SUCCESS)
        return FAILURE;

    while (1) {
        nfds = epoll_wait(worker->epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            return FAILURE;
        }
        for (int n = 0; n < nfds; n++) {
            // Periodically asking for work when worker is in IDLE state
            LOCK(&worker->mu);
            task_state_t _state = worker->state;
            UNLOCK(&worker->mu);
            if (events[n].data.fd == worker->timerfd && _state == IDLE) {
                uint64_t expiration = 0;
                read(worker->timerfd, &expiration, sizeof(expiration));
                req_work(worker);
            }
            if (events[n].data.fd == worker->coord_fd) {
                ret = event_handler(worker);
                if (ret != SUCCESS) return ret;
            }
        }
    }
    return SUCCESS;
}

int main(const int argc, char *const argv[])
{
    int ret = SUCCESS;
    worker_t worker = {
        .plug_path = "./plug.so",
        .coord_addr = DEFAULT_COORD_ADDR,
        .coord_port = DEFAULT_COORD_PORT,
        .mu = PTHREAD_MUTEX_INITIALIZER,
    };

    worker.state = IDLE;
    if (pthread_mutex_init(&worker.mu, NULL) != 0) {
        fprintf(stderr, "Could not initialise mutex\n");
        return FAILURE;
    }

    if (parse_arg(argc, argv, &worker) == FAILURE
        || load_mr_plugs(worker.plug_path, &worker) == FAILURE
        || connect_to_coord(&worker) == FAILURE)
        return FAILURE;
    ret = work(&worker);
    end_coord(&worker, NULL);
    return ret;
}
