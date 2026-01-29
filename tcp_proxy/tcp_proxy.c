#include "tcp_proxy.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 4096

static void pipe_stream(int a, int b)
{
    char buf[BUF_SIZE];
    fd_set fds;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(a, &fds);
        FD_SET(b, &fds);

        int maxfd = (a > b ? a : b) + 1;
        if (select(maxfd, &fds, NULL, NULL, NULL) <= 0)
            break;

        if (FD_ISSET(a, &fds)) {
            int n = read(a, buf, sizeof(buf));
            if (n <= 0) break;
            write(b, buf, n);
        }

        if (FD_ISSET(b, &fds)) {
            int n = read(b, buf, sizeof(buf));
            if (n <= 0) break;
            write(a, buf, n);
        }
    }
}

int tcp_proxy_run(const tcp_proxy_cfg_t *cfg)
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in laddr = {
        .sin_family = AF_INET,
        .sin_port = htons(cfg->listen_port),
        .sin_addr.s_addr = cfg->listen_ip ?
            inet_addr(cfg->listen_ip) : INADDR_ANY
    };

    bind(lfd, (struct sockaddr*)&laddr, sizeof(laddr));
    listen(lfd, 16);

    if (cfg->verbose)
        printf("[*] listen %s:%d -> %s:%d\n",
               cfg->listen_ip ? cfg->listen_ip : "0.0.0.0",
               cfg->listen_port,
               cfg->target_ip,
               cfg->target_port);

    while (1) {
        int cfd = accept(lfd, NULL, NULL);

        int rfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in raddr = {
            .sin_family = AF_INET,
            .sin_port = htons(cfg->target_port)
        };
        inet_pton(AF_INET, cfg->target_ip, &raddr.sin_addr);

        if (connect(rfd, (struct sockaddr*)&raddr, sizeof(raddr)) < 0) {
            close(cfd);
            close(rfd);
            continue;
        }

        pipe_stream(cfd, rfd);

        close(cfd);
        close(rfd);
    }
}
