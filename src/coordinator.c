#include "common.h"
#include "coordinator.h"

void cleanup_tcp_socket(tcp_sock_t *socket)
{
    if (!socket) return;
    if (socket->fd != -1) close(socket->fd);
    free(socket);
}

tcp_sock_t *tcp_listen(const uint16_t port)
{
    tcp_sock_t *sock = malloc(sizeof(tcp_sock_t));
    if (sock == NULL) return NULL;

    sock->addr.sin_family = AF_INET;
    sock->addr.sin_port = htons(port);
    sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == -1) {
        cleanup_tcp_socket(sock);
        perror("");
        return NULL;
    }
    if (bind(sock->fd, (struct sockaddr *)&sock->addr, sizeof(sock->addr))
        == -1) {
        cleanup_tcp_socket(sock);
        perror("");
        return NULL;
    }

    if (listen(sock->fd, 10) == -1) {
        cleanup_tcp_socket(sock);
        perror("");
        return NULL;
    }
    printf("listening for connection...\n");
    return sock;
}

tcp_sock_t *accept_connexion(tcp_sock_t *server)
{
    tcp_sock_t *client_sock = malloc(sizeof(tcp_sock_t));
    assert(client_sock != NULL && "no memory avail");

    // socklen_t client_addr_len = sizeof(client_sock->addr);
    socklen_t client_addr_len = 0;
    client_sock->fd = accept(
        server->fd, (struct sockaddr *)&client_sock->addr, &client_addr_len);
    if (client_sock->fd < 0) {
        perror("");
        free(client_sock);
        return NULL;
    }
    // #ifdef POLLERR
    //   printf("POLLER DEFINED\n");
    //   int flags = fcntl(client_sock->fd, F_GETFL);
    //   assert(flags > 0 && "issue getflags for file descriptor");
    //   flags |= O_NONBLOCK;
    //
    //   int new_fd = fcntl(client_sock->fd, F_SETFL, flags);
    //   assert(new_fd != -1 && "issue setting O_NONBLOCK for file descriptor");
    //   client_sock->fd = new_fd;
    // #endif
    printf("someone is connected...\n");
    return client_sock;
}

int on_read(network_ctx_t *ctx, void *data)
{
    int cli_fd = *((int *)data);
    printf("reading from client: %d\n", cli_fd);

    char buff[256] = { 0 };
    ssize_t msg_len = 0;

    while ((msg_len = recv(cli_fd, &buff, sizeof(buff), 0)) != 0) {
        if (msg_len == -1) {
            perror("");
            return FAILURE;
        }
        if (strcmp(buff, "exit\n") == 0) {
            printf("client has %d exited", cli_fd);
            return SUCCESS;
        }
        write(1, buff, msg_len);
        write(cli_fd, buff, msg_len);
    }
    // TODO: Check if session is terminated...
    if (msg_len == 0) {
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, cli_fd, NULL);
        printf("deleting client %d\n", cli_fd);
    }

    return SUCCESS;
}

int on_accept(network_ctx_t *ctx, void __attribute__((unused)) * data)
{
    printf("Detecting connection\n");
    tcp_sock_t *client_sock = accept_connexion(ctx->serv);
    if (!client_sock) {
        return FAILURE;
    }

    handler_t *read_handler = malloc(sizeof(handler_t));
    assert(read_handler != NULL && "no more space available");
    read_handler->fd = client_sock->fd;
    read_handler->callback = &on_read;
    read_handler->data = &client_sock->fd;

    struct epoll_event client_event = {
        .events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP,
        .data.ptr = read_handler,
    };

    // printf("appending to dyn arr\n");
    // da_append(&ctx->client_set, client_sock);

    if (epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, client_sock->fd, &client_event)
        == -1) {
        perror("");
        return FAILURE;
    }
    printf("adding client's fd to interest list\n");
    return SUCCESS;
}

// Basically our event loop
int serve(network_ctx_t *ctx)
{
    int nfds = 0;
    struct epoll_event ev, events[MAX_EVENTS];

    ctx->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx->epfd == -1) {
        perror("");
        return FAILURE;
    }
    ev.events = EPOLLIN;

    handler_t connection_handler = {
        .fd = ctx->serv->fd,
        .data = NULL,
        .callback = &on_accept,
    };
    ev.data.ptr = &connection_handler;

    if (epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->serv->fd, &ev) == -1) {
        perror("");
        return FAILURE;
    }

    while (1) {
        nfds = epoll_wait(ctx->epfd, events, MAX_EVENTS, -1);
        printf("---> events received %d\n", nfds);
        if (nfds == -1) {
            perror("");
            return FAILURE;
        }
        for (int n = 0; n < nfds; n++) {
            printf("events [%d] type %#x\n", n, events[n].events);
            if (events[n].data.ptr) {
                handler_t *handle = (handler_t *)events[n].data.ptr;
                assert(handle->callback(ctx, handle->data) != FAILURE
                    && "error callback");
                printf("nbr of clients %ld\n", ctx->client_set.count);
            }
        }
    }
    // foreach (tcp_sock_t *, cli, &ctx->client_set) {
    //     cleanup_tcp_socket(*cli);
    // }
    return SUCCESS;
}

int run_server(void)
{
    int ret_val = SUCCESS;
    network_ctx_t *net_ctx = malloc(sizeof(network_ctx_t));
    assert(net_ctx != NULL && "Could not allocate network context");

    net_ctx->serv = tcp_listen(COORDINATOR_PORT);
    memset(&net_ctx->client_set, 0, sizeof(client_t));
    if (!net_ctx->serv)
        ret_val = FAILURE;
    else
        ret_val = serve(net_ctx); // TBD: maybe run it in its own thread?

    cleanup_tcp_socket(net_ctx->serv);
    free(net_ctx);
    return ret_val;
}

int main(void)
{
    return run_server();
}
