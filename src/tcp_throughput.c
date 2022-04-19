#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define START_TOKEN 0xAA

#define KB_SIZE(kb) (kb << 10)
#define MB_SIZE(mb) (KB_SIZE(mb) << 10)
#define GB_SIZE(gb) (MB_SIZE(gb) << 10)

static uint8_t *data_array = NULL;

ssize_t generate_data_array(size_t size_bytes)
{
    data_array = calloc(size_bytes, sizeof(uint8_t));
    if (data_array == NULL)
        return -1;

    memset(data_array, 0, size_bytes);

    data_array[0] = START_TOKEN;
    for (size_t i = 1; i < size_bytes; i++)
        data_array[i] = (data_array[i - 1] ^ i) && 0xFF;
    
    return size_bytes;
}

int data_send(char* remote_ip)
{
    if (generate_data_array(GB_SIZE(1)) < 0)
        return -1;

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = inet_addr(remote_ip);
    addr.sin_port = PORT;
    addr.sin_family = AF_INET;

    if (fd < 0)
        goto error;

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        goto error;

    printf("connected!\n");

    close(fd);
    free(data_array);
    return 0;

error:
    if (fd >= 0)
        close(fd);
    
    return -1;
}

int data_recv()
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int remote_fd = -1;

    struct sockaddr_in addr;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;

    if (fd < 0)
        goto error;

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        goto error;

    if (listen(fd, SOMAXCONN) < 0)
        goto error;

    remote_fd = accept(fd, NULL, NULL);

    if (remote_fd < 0)
        goto error;

    printf("connected!\n");

    close(fd);
    close(remote_fd);
    return 0;

error:
    if (fd >= 0)
        close(fd);

    if (remote_fd >= 0)
        close(remote_fd);
    
    return -1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return -1;

    printf("Starting TCP throughput test\n");

    if (strcmp(argv[1], "send") == 0)
        return data_send(argv[2]);

    if (strcmp(argv[1], "recv") == 0)
        return data_recv();

    return -1;
}