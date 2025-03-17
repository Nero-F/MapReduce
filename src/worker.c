#include "common.h"
#include "worker.h"
#include <stdlib.h>
#include <stdio.h>

#define COORD_ADDR "localhost"
#define COORD_PORT "4242"

static void display_help(void)
{
    printf("Usage: worker [options(s)]\n");
    printf("\t-H, --host\thost address of the coordinator\n");
    printf("\t-P, --port\thost port of the coordinator\n");
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

static int end_coord(worker_t worker, int sockfd, struct addrinfo *coord_info,
    char *err_msg, int ret_val)
{
    if (err_msg != NULL) perror(err_msg);
    if (worker.plug_handle) dlclose(worker.plug_handle);
    if (sockfd != -1) close(sockfd);
    if (coord_info != NULL) freeaddrinfo(coord_info);
    return ret_val;
}

int connect_to_coord(worker_t worker)
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

    if (sockfd == -1)
        return end_coord(worker, sockfd, coord_info, "socket", FAILURE);
    if (bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1)
        return end_coord(worker, sockfd, coord_info, "bind", FAILURE);

    ret_gai = getaddrinfo(
        worker.coord_addr, worker.coord_port, &hints, &coord_info);

    if (ret_gai != 0) {
        fprintf(stderr, "[ERROR]: %s\n", gai_strerror(ret_gai));
        return end_coord(worker, sockfd, coord_info, NULL, FAILURE);
    }
    if (connect(sockfd, coord_info->ai_addr, coord_info->ai_addrlen) == -1)
        return end_coord(worker, sockfd, coord_info, "connect", FAILURE);

    // TODO: Periodically ask for work
    while (1) {
        sleep(3);
    }

    return end_coord(worker, sockfd, coord_info, NULL, SUCCESS);
}

int main(const int argc, char *const argv[])
{
    worker_t worker = {
        .plug_path = "./plug.so",
        .coord_addr = COORD_ADDR,
        .coord_port = COORD_PORT,
    };

    if (parse_arg(argc, argv, &worker) == FAILURE
        || load_mr_plugs(worker.plug_path, &worker) == FAILURE)
        return FAILURE;
    connect_to_coord(worker);
    return SUCCESS;
}
