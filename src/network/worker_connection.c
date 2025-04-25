#include "common.h"
#include "defer.h"
#include "frpc.h"
#include "worker.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>

#define RETRY_CALL(err_msg)                                                    \
    do {                                                                       \
        if (try_nbr < retry) {                                                 \
            try_nbr++;                                                         \
            goto try_call;                                                     \
        } else {                                                               \
            perror(err_msg);                                                   \
            return FAILURE;                                                    \
        }                                                                      \
    } while (0);

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

int connect_to_coord(worker_t *worker)
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
