#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define KiB_SIZE(kb) (kb << 10)
#define MiB_SIZE(mb) (KiB_SIZE(mb) << 10)
#define GiB_SIZE(gb) (MiB_SIZE(gb) << 10)

//#define DEBUG_PRINTS
#define PORT 8080
#define START_TOKEN 0xAA
#define DATA_ARRAY_SIZE GiB_SIZE(1)

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

int validate_buffers(uint8_t* buffA, uint8_t* buffB, size_t buffALength, size_t buffBLength)
{
    if (buffALength != buffBLength)
        return 0;

    for (size_t i = 0; i < buffALength; i++)
    {
        if ((buffA[i] ^ buffB[i]) != 0x00)
        {
            printf("Invalid token @ %zu [0x%x != 0x%x]\n", i, buffA[i], buffB[i]);
            return 0;
        }
    }

    return 1;
}

int data_send(char* remote_ip)
{
    if (generate_data_array(DATA_ARRAY_SIZE) < 0)
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

#ifdef DEBUG_PRINTS
    printf("connected!\n");
#endif

    struct timespec start_transfer;
    clock_gettime(CLOCK_REALTIME, &start_transfer);
    ssize_t res = send(fd, data_array, DATA_ARRAY_SIZE, 0);

    if (res < 0)
        goto error;

    size_t response_buffer_offset = 0;
    uint8_t* response_buffer = calloc(DATA_ARRAY_SIZE, sizeof(uint8_t));

    ssize_t recv_result = 0;
    do {
        recv_result = recv(fd, response_buffer + response_buffer_offset, DATA_ARRAY_SIZE - response_buffer_offset, 0);

        response_buffer_offset += recv_result;
        
#ifdef DEBUG_PRINTS
        printf("[send] received %ld/%d bytes\n", response_buffer_offset, DATA_ARRAY_SIZE);
#endif
    } while(recv_result > 0 && response_buffer_offset != DATA_ARRAY_SIZE);

    if (recv_result < 0)
    {
        free(response_buffer);
        goto error;
    }

    struct timespec end_transfer;
    clock_gettime(CLOCK_REALTIME, &end_transfer);

    if (validate_buffers(data_array, response_buffer, DATA_ARRAY_SIZE, DATA_ARRAY_SIZE) == 0)
    {
        printf("Buffers Invalid\n");
        goto error;
    }

    ssize_t seconds = end_transfer.tv_sec - start_transfer.tv_sec;
    ssize_t ns = (end_transfer.tv_nsec - start_transfer.tv_nsec);

    if (ns < 0)
    {
        ns += 1000000000;
        seconds--;
    }

    ssize_t ms = ns / 1000000;

    printf("Buffers Valid @ %lus:%lums\n", seconds, ms);

#ifdef DEBUG_PRINTS
    printf("Cleanup for send\n");
#endif

    close(fd);
    free(data_array);
    return 0;

error:
    if (fd >= 0)
        close(fd);

    free(data_array);    
    return -1;
}

int data_recv()
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int enable_reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse));

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

#ifdef DEBUG_PRINTS
    printf("[recv] connected!\n");
#endif

    size_t recv_buff_offset = 0;
    uint8_t* recv_buffer = calloc(DATA_ARRAY_SIZE, sizeof(uint8_t));

    ssize_t recv_result = 0;
    do {
        recv_result = recv(remote_fd, recv_buffer + recv_buff_offset, DATA_ARRAY_SIZE - recv_buff_offset, 0);

        recv_buff_offset += recv_result;
        
#ifdef DEBUG_PRINTS
        printf("[recv] received %ld/%d bytes\n", recv_buff_offset, DATA_ARRAY_SIZE);
#endif
    } while(recv_result > 0 && recv_buff_offset != DATA_ARRAY_SIZE);

#ifdef DEBUG_PRINTS
    printf("[recv] receiving done\n");
#endif

    if (recv_result < 0)
    {
        free(recv_buffer);
        goto error;
    }

    int send_result = send(remote_fd, recv_buffer, DATA_ARRAY_SIZE, 0);
    if (send_result < 0)
    {
        free(recv_buffer);
        goto error;
    }

    printf("Cleanup for recv\n");

    close(remote_fd);
    close(fd);
    return 0;

error:
    if (remote_fd >= 0)
        close(remote_fd);

    if (fd >= 0)
        close(fd);
    
    perror(NULL);

    return -1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return -1;

    if (argc == 4)
        printf("%s\tTCP THROUGHPUT\n", argv[3]);
    else
        printf("---\tTCP THROUGHPUT\t---\n");

    if (strcmp(argv[1], "send") == 0)
        return data_send(argv[2]);

    if (strcmp(argv[1], "recv") == 0)
        return data_recv();

    return -1;
}