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

#define RETRY_SEND(err_msg)                                                    \
    do {                                                                       \
        if (try_nbr < retry) {                                                 \
            try_nbr++;                                                         \
            goto try_send;                                                     \
        } else {                                                               \
            perror(err_msg);                                                   \
            return FAILURE;                                                    \
        }                                                                      \
    } while (0);

int call(opcode_t op, worker_t *worker, payload_t *resp, uint retry)
{
    static size_t id = 0;
    int sockfd = worker->coord_fd;
    struct addrinfo *coord_info = worker->coord_info;
    uint try_nbr = 0;

    msg_t msg = {
        .ack = ACK,
        .type = REQUEST,
        .payload = { .id = id++, .op = op, .data = resp->data },
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
    *resp = msg.payload;
    return SUCCESS;
}

int _send(opcode_t op, worker_t *worker, inner_data_u data, uint retry)
{
    static size_t id = 0;
    int sockfd = worker->coord_fd;
    struct addrinfo *coord_info = worker->coord_info;
    uint try_nbr = 0;

    msg_t msg = {
        .ack = ACK,
        .type = REQUEST,
        .payload = { .id = id++, .op = op, .data = data },
    };

try_send:
    if (sendto(sockfd, &msg, sizeof(msg_t), 0, coord_info->ai_addr,
            coord_info->ai_addrlen)
        == -1)
        RETRY_SEND("sendto");
    return SUCCESS;
}

int _recv(worker_t *worker, payload_t *resp)
{
    int sockfd = worker->coord_fd;
    struct addrinfo *coord_info = worker->coord_info;
    msg_t msg = { 0 };
    int ret_recv = 0;
    if ((ret_recv = recvfrom(sockfd, &msg, sizeof(msg_t), 0,
             coord_info->ai_addr, &coord_info->ai_addrlen))
        == -1) {
        perror("recvfrom");
        return FAILURE;
    } else if (ret_recv == 0) {
        printf("[[COORDINATOR EXIT]]...\n");
        return TERMINATE;
    }

    *resp = msg.payload;
    return SUCCESS;
}

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
