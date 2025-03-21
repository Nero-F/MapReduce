#include "common.h"
#include "coordinator.h"
#include "frpc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

void cleanup_tcp_socket(tcp_sock_t *socket)
{
    if (!socket) return;
    if (socket->fd != -1) close(socket->fd);
    free(socket);
}

static int non_blocking_sock(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }
    flags |= O_NONBLOCK;

    int new_fd = fcntl(sockfd, F_SETFL, flags);
    if (new_fd == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

static tcp_sock_t *accept_connexion(tcp_sock_t *server)
{
    tcp_sock_t *client_sock = malloc(sizeof(tcp_sock_t));
    assert(client_sock != NULL && "no memory avail");

    socklen_t client_addr_len = 0;
    client_sock->fd = accept(
        server->fd, (struct sockaddr *)&client_sock->addr, &client_addr_len);
    if (client_sock->fd < 0) {
        perror("accept");
        free(client_sock);
        return NULL;
    }
    if (non_blocking_sock(client_sock->fd) == -1) return NULL;
    printf("accepting fd::: %d...\n", client_sock->fd);
    return client_sock;
}

static tcp_sock_t *tcp_listen(const uint16_t port)
{
    tcp_sock_t *sock = malloc(sizeof(tcp_sock_t));
    if (sock == NULL) return NULL;

    sock->addr.sin_family = AF_INET;
    sock->addr.sin_port = htons(port);
    sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == -1) {
        cleanup_tcp_socket(sock);
        perror("socket");
        return NULL;
    }
    if (non_blocking_sock(sock->fd) == -1) return NULL;
    if (bind(sock->fd, (struct sockaddr *)&sock->addr, sizeof(sock->addr))
        == -1) {
        cleanup_tcp_socket(sock);
        perror("bind");
        return NULL;
    }

    if (listen(sock->fd, 10) == -1) {
        cleanup_tcp_socket(sock);
        perror("listen");
        return NULL;
    }
    printf("listening for connection...\n");
    return sock;
}

static int __attribute__((unused)) on_read_echo(network_ctx_t *ctx, void *data)
{
    tcp_sock_t *client = (tcp_sock_t *)data;
    int cli_fd = client->fd;
    printf("reading from client: %d\n", cli_fd);

    char buff[256] = { 0 };
    ssize_t msg_len = 0;

    while ((msg_len = recv(cli_fd, &buff, sizeof(buff), 0)) > 0) {
        if (strcmp(buff, "exit\r\n") == 0) {
            return SUCCESS;
        }
        write(1, buff, msg_len);
        write(cli_fd, buff, msg_len);
    }
    if (msg_len == -1 && errno != EAGAIN) {
        close(cli_fd);
        perror("recv");
        return FAILURE;
    } else if (msg_len == 0) {
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, cli_fd, NULL);
        close(cli_fd);
        printf("client has %d exited, eleting client \n", cli_fd);
    }
    return SUCCESS;
}

static int __attribute__((unused)) on_recv_worker(
    network_ctx_t *ctx, void *data)
{
    tcp_sock_t *client = (tcp_sock_t *)data;
    int cli_fd = client->fd;
    printf("reading from worker: %d\n", cli_fd);

    request_t req = { 0 };
    ssize_t msg_len = 0;

    while ((msg_len = recv(cli_fd, (byte *)&req, sizeof(req), 0)) > 0) {
        if (req.ack != ACK) {
            fprintf(stderr, "ACK not valid\n");
            close(cli_fd);
            return FAILURE;
        }
        printf("[WORKER:%d][REQ:%d] received with opcode %d ", cli_fd, req.id,
            req.op);
        // response_t __attribute__((unused)) resp = process_req(req,
        // data->coord);
    }
    // ignore EAGAIN cause we're using non blocking socket
    if (msg_len == -1 && errno != EAGAIN) {
        perror("recv");
        close(cli_fd);
        return FAILURE;
    }
    if (msg_len == 0) {
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, cli_fd, NULL);
        close(cli_fd);
        printf("[WORKER:%d] disconnected\n", cli_fd);
    }
    return SUCCESS;
}

static int on_accept(network_ctx_t *ctx, __attribute__((unused)) void *data)
{
    printf("Detecting connection\n");
    tcp_sock_t *client_sock = accept_connexion(ctx->serv);
    if (!client_sock) return FAILURE;

    handler_t *read_handler = malloc(sizeof(handler_t));
    assert(read_handler != NULL && "read_handler no more space available");
    read_handler->fd = client_sock->fd;
    read_handler->callback = &on_recv_worker;
    read_handler->data = client_sock;

    printf("mes couilles fd:::::: %d\n", read_handler->fd);
    struct epoll_event client_event = {
        .events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP,
        .data.ptr = read_handler,
    };

    if (epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, client_sock->fd, &client_event)
        == -1) {
        free(read_handler);
        perror("epoll_ctl");
        return FAILURE;
    }
    printf("adding client's fd to interest list\n");
    return SUCCESS;
}

// Basically our event loop
int serve(network_ctx_t *ctx, coordinator_t __attribute__((unused)) coord)
{
    int nfds = 0;
    struct epoll_event ev, events[MAX_EVENTS];

    ctx->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx->epfd == -1) {
        perror("epoll_create1");
        return FAILURE;
    }

    ev.events = EPOLLIN;
    ev.data.ptr = &(handler_t) {
        .data = NULL,
        .callback = &on_accept,
    };

    if (epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->serv->fd, &ev) == -1) {
        perror("epoll_ctl");
        return FAILURE;
    }

    while (1) {
        nfds = epoll_wait(ctx->epfd, events, MAX_EVENTS, -1);
        printf("---> %d events received \n", nfds);
        if (nfds == -1) {
            perror("epoll_wait");
            return FAILURE;
        }
        for (int n = 0; n < nfds; n++) {
            printf("events [%d] type %#x\n", n, events[n].events);
            // if (events[n].data.fd == ctx->serv->fd) {
            //     on_accept(ctx, NULL);
            // } else {
            assert(events[n].data.ptr != NULL);
            handler_t *handle = (handler_t *)events[n].data.ptr;
            printf("handling event for fd:::::: %d\n", handle->fd);
            handle->callback(ctx, handle->data);
            // }
        }
    }
    return SUCCESS;
}

// TODO: maybe abstract away the event loop to make the program more portable
// (kqueue for bsd or use of libuv)
int run_server(coordinator_t coord)
{
    int ret_val = SUCCESS;
    network_ctx_t *net_ctx = malloc(sizeof(network_ctx_t));
    assert(net_ctx != NULL && "Could not allocate network context");

    net_ctx->serv = tcp_listen(coord.running_port);
    if (!net_ctx->serv)
        ret_val = FAILURE;
    else
        ret_val = serve(net_ctx, coord); // TBD: maybe run it in its own thread?

    cleanup_tcp_socket(net_ctx->serv);
    free(net_ctx);
    return ret_val;
}
