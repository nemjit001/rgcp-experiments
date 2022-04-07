#include <unistd.h>
#include <time.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080

static in_addr_t g_recv_addr = -1;

int data_send()
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;

    if (fd < 0)
        return -1;

    while (g_recv_addr == -1);
    addr.sin_addr.s_addr = g_recv_addr;

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        goto error;
    
    // send data until done
    
    return 0;

error:
    close(fd);

    return -1;
}

int data_recv()
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int remote = -1;
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;

    if (fd < 0 || bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(fd, SOMAXCONN) < 0)
        goto error;

    g_recv_addr = addr.sin_addr.s_addr;

    remote = accept(fd, NULL, NULL);
    if (remote < 0)
        goto error;

    // receive data until done
    
    return 0;

error:
    if (fd >= 0)
        close(fd);

    if (remote >= 0)
        close(remote);
    
    return -1;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return -1;

    if (strcmp(argv[1], "send") == 0)
        return data_send();

    if (strcmp(argv[1], "recv") == 0)
        return data_recv();

    return -1;
}