#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define KB_SIZE(kb) (kb << 10)
#define MB_SIZE(mb) (KB_SIZE(mb) << 10)
#define GB_SIZE(gb) (MB_SIZE(gb) << 10)

// #define DEBUG_PRINTS
#define PORT 8080
#define START_TOKEN 0xAA
#define DATA_ARRAY_SIZE KB_SIZE(2)

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
            return 0;
    }

    return 1;
}

int data_send(char* remote_ip)
{
    size_t data_array_size = DATA_ARRAY_SIZE;

    if (generate_data_array(data_array_size) < 0)
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

    size_t stride = KB_SIZE(1);
    size_t array_idx = 0;

    struct timespec start_transfer;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_transfer);
    while (array_idx < data_array_size)
    {
        size_t send_buff_size = stride;

        if (array_idx + stride > data_array_size)
            send_buff_size = data_array_size - array_idx;

        int res = send(fd, (data_array + array_idx), send_buff_size, 0);
        if (res < 0)
            goto error;

#ifdef DEBUG_PRINTS
        printf("[Send][%zu bytes]\n", send_buff_size);
#endif

        array_idx += send_buff_size;
    }

    size_t response_buffer_size = KB_SIZE(1);
    size_t response_buffer_idx = 0;
    uint8_t* response_buffer = calloc(response_buffer_size, sizeof(uint8_t));
    int response_res = recv(fd, response_buffer, stride, 0);

    while(response_res > 0 && response_buffer_size < DATA_ARRAY_SIZE)
    {
#ifdef DEBUG_PRINTS
        printf(
            "[Send][Echo response][%zu -> %zu][idx: %zu]\n", 
            response_buffer_size, 
            response_buffer_size + stride, 
            response_buffer_idx
        );
#endif

        uint8_t* temp = realloc(response_buffer, response_buffer_size + stride);

        if (temp == NULL)
        {
            free(response_buffer);
            goto error;
        }

        response_buffer_idx += stride;
        response_buffer_size += stride;
        response_buffer = temp;

        response_res = recv(fd, (response_buffer + response_buffer_idx), stride, 0);
    }
    struct timespec end_transfer;
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_transfer);

    if (validate_buffers(data_array, response_buffer, data_array_size, response_buffer_size) == 0)
    {
        printf("Buffers Invalid\n");
    }

    size_t seconds = end_transfer.tv_sec - start_transfer.tv_sec;
    size_t ms = (end_transfer.tv_nsec - start_transfer.tv_nsec) / 1000000;
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
    printf("connected!\n");
#endif

    size_t stride = KB_SIZE(1);
    size_t buffer_idx = 0;
    size_t buffer_size = KB_SIZE(1);
    uint8_t* recv_buffer = calloc(stride, sizeof(uint8_t));
    int recv_result = recv(remote_fd, recv_buffer, stride, 0);
    while (recv_result > 0 && buffer_size < DATA_ARRAY_SIZE)
    {
#ifdef DEBUG_PRINTS
        printf("[Recv][%zu -> %zu][idx: %zu]\n", buffer_size, buffer_size + stride, buffer_idx);
#endif
        uint8_t *temp = realloc(recv_buffer, buffer_size + stride);
        
        if (temp == NULL)
        {
            free(recv_buffer);
            goto error;
        }

        buffer_idx += buffer_size;
        buffer_size += stride;
        recv_buffer = temp;

        recv_result = recv(remote_fd, (recv_buffer + buffer_idx), stride, 0);
    }

#ifdef DEBUG_PRINTS
    printf("[Recv][Total buffer size: %zu bytes]\n", buffer_size);
#endif

    buffer_idx = 0;
    while (buffer_idx < buffer_size)
    {
        size_t send_buff_size = stride;

        if (buffer_idx + stride > buffer_size)
            send_buff_size = buffer_size - buffer_idx;

        int res = send(remote_fd, (recv_buffer + buffer_idx), send_buff_size, 0);
        if (res < 0)
            goto error;

#ifdef DEBUG_PRINTS
        printf("[Recv][Echoing %zu bytes]\n", send_buff_size);
#endif

        buffer_idx += send_buff_size;
    }

#ifdef DEBUG_PRINTS
    printf("Cleanup for recv\n");
#endif

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