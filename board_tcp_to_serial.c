#include <arpa/inet.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>

#define MAGIC 0x55AA
#define BUF   1024

#define DBG(fmt, ...) \
    fprintf(stderr, "[BOARD] " fmt "\n", ##__VA_ARGS__)

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

int connect_http()
{
    int tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(80)
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(tcp, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect tcp");
        close(tcp);
        return -1;
    }

    DBG("connected to http.server :80");
    return tcp;
}

int main()
{
    DBG("starting board proxy");

    int ser = open_serial("/dev/ttyUSB0");
    if (ser < 0) return 1;

    uint8_t buf[BUF];

    while (1) {
        uint16_t magic, len;

        /* ===== 等待一帧串口请求 ===== */
        int r = readn(ser, &magic, 2);
        DBG("serial read magic: %d", r);
        if (r <= 0) break;

        if (magic != MAGIC) {
            DBG("bad magic: 0x%04x", magic);
            continue;
        }

        r = readn(ser, &len, 2);
        DBG("serial read len: %d (%u)", r, len);
        if (r <= 0) break;

        r = readn(ser, buf, len);
        DBG("serial read payload: %d", r);
        if (r <= 0) break;

        /* ===== 新建 TCP 连接 ===== */
        int tcp = connect_http();
        if (tcp < 0)
            continue;

        /* 把 request 写给 http.server */
        write(tcp, buf, len);
        DBG("wrote %u bytes to tcp", len);

        /* ===== 把 http.response 读完并回传串口 ===== */
        while (1) {
            int n = read(tcp, buf, BUF);
            DBG("tcp read: %d", n);
            if (n <= 0)
                break;

            uint16_t m = MAGIC;
            uint16_t l = n;
            write(ser, &m, 2);
            write(ser, &l, 2);
            write(ser, buf, n);

            DBG("sent frame to serial, len=%d", n);
        }

        close(tcp);
        DBG("tcp closed, wait next request");
    }

    DBG("exit");
    return 0;
}
