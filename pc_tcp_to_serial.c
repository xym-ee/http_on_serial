#include <arpa/inet.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>

#define MAGIC 0x55AA
#define BUF   1024

#define DBG(fmt, ...) \
    fprintf(stderr, "[PC] " fmt "\n", ##__VA_ARGS__)

int readn(int fd, void *buf, int n)
{
    int left = n;
    char *p = buf;

    while (left > 0) {
        int r = read(fd, p, left);
        if (r <= 0)
            return -1;
        left -= r;
        p += r;
    }
    return n;
}

int open_serial(const char *dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open serial");
        return -1;
    }

    struct termios t;
    tcgetattr(fd, &t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag = CS8 | CLOCAL | CREAD;
    t.c_iflag = t.c_oflag = t.c_lflag = 0;
    tcsetattr(fd, TCSANOW, &t);

    DBG("serial opened: %s", dev);
    return fd;
}

int main()
{
    DBG("starting pc proxy");

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in laddr = {
        .sin_family = AF_INET,
        .sin_port = htons(7777),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(lfd, (struct sockaddr*)&laddr, sizeof(laddr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(lfd, 5) < 0) {
        perror("listen");
        return 1;
    }

    DBG("listening on 7777");

    int ser = open_serial("/dev/ttyUSB0");
    if (ser < 0) return 1;

    uint8_t buf[BUF];
    fd_set fds;

    /* ===== 核心改动：支持多次浏览器连接 ===== */
    while (1) {
        int tcp = accept(lfd, NULL, NULL);
        if (tcp < 0) {
            perror("accept");
            continue;
        }

        DBG("browser connected");

        while (1) {
            FD_ZERO(&fds);
            FD_SET(tcp, &fds);
            FD_SET(ser, &fds);

            int maxfd = (tcp > ser ? tcp : ser) + 1;
            if (select(maxfd, &fds, NULL, NULL, NULL) <= 0) {
                DBG("select error");
                break;
            }

            /* TCP -> Serial */
            if (FD_ISSET(tcp, &fds)) {
                int n = read(tcp, buf, BUF);
                DBG("tcp read: %d", n);
                if (n <= 0)
                    break;

                uint16_t magic = MAGIC;
                uint16_t len = n;

                write(ser, &magic, 2);
                write(ser, &len, 2);
                write(ser, buf, n);

                DBG("sent frame to serial, len=%d", n);
            }

            /* Serial -> TCP */
            if (FD_ISSET(ser, &fds)) {
                uint16_t magic, len;

                int r = readn(ser, &magic, 2);
                DBG("serial read magic: %d", r);
                if (r <= 0)
                    break;

                if (magic != MAGIC) {
                    DBG("bad magic: 0x%x", magic);
                    break;
                }

                r = readn(ser, &len, 2);
                DBG("serial read len: %d (%u)", r, len);
                if (r <= 0)
                    break;

                r = readn(ser, buf, len);
                DBG("serial read payload: %d", r);
                if (r <= 0)
                    break;

                write(tcp, buf, r);
                DBG("wrote %d bytes back to tcp", r);
            }
        }

        DBG("browser disconnected");
        close(tcp);
    }

    DBG("exit");
    return 0;
}

