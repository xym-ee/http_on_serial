#include "tcp_proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void parse_addr(const char *s, const char **ip, int *port)
{
    char *p = strchr(s, ':');
    if (!p) {
        *ip = NULL;
        *port = atoi(s);
        return;
    }
    *p = '\0';
    *ip = s;
    *port = atoi(p + 1);
}

int main(int argc, char *argv[])
{
    tcp_proxy_cfg_t cfg = {0};

    static struct option opts[] = {
        {"listen",  required_argument, 0, 'l'},
        {"target",  required_argument, 0, 't'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0,0,0,0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "l:t:vh", opts, NULL)) != -1) {
        switch (c) {
        case 'l':
            parse_addr(optarg, &cfg.listen_ip, &cfg.listen_port);
            break;
        case 't':
            parse_addr(optarg, &cfg.target_ip, &cfg.target_port);
            break;
        case 'v':
            cfg.verbose = 1;
            break;
        case 'h':
        default:
            printf("Usage: %s -l [ip:]port -t ip:port [-v]\n", argv[0]);
            return 0;
        }
    }

    if (!cfg.target_ip || !cfg.listen_port) {
        printf("invalid arguments\n");
        return -1;
    }

    return tcp_proxy_run(&cfg);
}
