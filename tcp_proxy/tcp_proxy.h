#pragma once

typedef struct {
    const char *listen_ip;
    int listen_port;
    const char *target_ip;
    int target_port;
    int verbose;
} tcp_proxy_cfg_t;

int tcp_proxy_run(const tcp_proxy_cfg_t *cfg);
