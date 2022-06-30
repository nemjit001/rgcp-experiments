#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #define RGCP_SOCKET_HEARTBEAT_PERIOD_SECONDS 30

#include <pthread.h>
#include <rgcp/rgcp.h>

#define KiB_SIZE(kb) (kb << 10)
#define MiB_SIZE(mb) (KiB_SIZE(mb) << 10)
#define GiB_SIZE(gb) (MiB_SIZE(gb) << 10)

#define GROUP_NAME "RGCP_TP_TEST"
#define SENDER_MSG "SND"

#define WAIT_TIMEOUT_SECONDS 10
#define DATA_ARRAY_START_TOKEN 0xAA
#define DATA_ARRAY_SIZE(peer_count) ((uint32_t)(GiB_SIZE(1) / (peer_count)))

struct client_buff_data
{
    int m_fd;
    uint8_t* m_p_buffer;
    size_t m_buffersize;
};

static struct client_buff_data* g_p_client_buffers = NULL;
static size_t g_client_buff_size = 0;

static uint8_t* g_p_data_array = NULL;
static int g_send_fd = -1;
static int g_b_recv_done = 0;

int create_socket(char* mw_ipaddr, uint16_t port)
{
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = inet_addr(mw_ipaddr);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    int sockfd = rgcp_socket(AF_INET, (struct sockaddr*)&addr, sizeof(addr));
    return sockfd;
}

int connect_to_group(int fd, char* p_groupname)
{
    rgcp_group_info_t **pp_groupinfos = NULL;
    
    ssize_t groupcount = rgcp_discover_groups(fd, &pp_groupinfos);

    if (groupcount < 0)
        return -1;
    
    rgcp_group_info_t* p_group = NULL;
    uint32_t max_hash = 0;
    for (ssize_t i = 0; i < groupcount; i++)
    {
        if (strcmp(pp_groupinfos[i]->m_pGroupName, p_groupname) == 0 && pp_groupinfos[i]->m_groupNameHash > max_hash)
        {
            max_hash = pp_groupinfos[i]->m_groupNameHash;
            p_group = pp_groupinfos[i];
        }
    }

    if (p_group == NULL || (rgcp_connect(fd, *p_group) < 0))
    {
        rgcp_free_group_infos(&pp_groupinfos, groupcount);
        return -1;
    }

    rgcp_free_group_infos(&pp_groupinfos, groupcount);    
    return 0;
}

struct timespec get_delta(struct timespec A, struct timespec B)
{
    struct timespec res;
    res.tv_sec = B.tv_sec - A.tv_sec;
    res.tv_nsec = B.tv_nsec - A.tv_nsec;

    if (res.tv_nsec < 0)
    {
        res.tv_nsec += 1000000000;
        res.tv_sec--;
    }

    return res;
}

int await_client_count(int fd, int expected_count, int timeout_seconds)
{
    struct timespec start_time, curr_lap, elapsed_time;
    memset(&start_time, 0, sizeof(start_time));
    memset(&curr_lap, 0, sizeof(curr_lap));
    memset(&elapsed_time, 0, sizeof(elapsed_time));

    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    while(rgcp_peer_count(fd) != (expected_count - 1) && elapsed_time.tv_sec < timeout_seconds)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &curr_lap);
        elapsed_time = get_delta(start_time, curr_lap);
    }

    return (rgcp_peer_count(fd) == (expected_count - 1));
}

ssize_t generate_data_array(size_t size_bytes)
{
    srand(time(NULL));

    g_p_data_array = calloc(size_bytes, sizeof(uint8_t));
    if (g_p_data_array == NULL)
        return -1;

    memset(g_p_data_array, 0, size_bytes);

    g_p_data_array[0] = DATA_ARRAY_START_TOKEN;
    for (size_t i = 1; i < size_bytes; i++)
        g_p_data_array[i] = (g_p_data_array[i - 1] ^ i) & 0xFF;
    
    return size_bytes;
}

int exists_in_client_array(int fd)
{
    for (size_t i = 0; i < g_client_buff_size; i++)
    {
        if (fd == g_p_client_buffers[i].m_fd)
            return 1;
    }

    return 0;
}

void load_into_client_array(int fd, uint8_t* p_data_buff, size_t buff_size)
{
    if (exists_in_client_array(fd))
    {
        for (size_t i = 0; i < g_client_buff_size; i++)
        {
            if (g_p_client_buffers[i].m_fd != fd)
                continue;

            if (!g_p_client_buffers[i].m_p_buffer)
            {
                g_p_client_buffers[i].m_p_buffer = calloc(buff_size, sizeof(uint8_t));
                memcpy(g_p_client_buffers[i].m_p_buffer, p_data_buff, buff_size);
                g_p_client_buffers[i].m_buffersize = buff_size;
            }
            else
            {
                g_p_client_buffers[i].m_p_buffer = realloc(g_p_client_buffers[i].m_p_buffer, (g_p_client_buffers[i].m_buffersize + buff_size) * sizeof(uint8_t));
                memcpy(g_p_client_buffers[i].m_p_buffer + g_p_client_buffers[i].m_buffersize, p_data_buff, buff_size);
                g_p_client_buffers[i].m_buffersize += buff_size;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < g_client_buff_size; i++)
        {
            if (!g_p_client_buffers[i].m_fd == 0)
                continue;

            g_p_client_buffers[i].m_fd = fd;
            g_p_client_buffers[i].m_p_buffer = calloc(buff_size, sizeof(uint8_t));
            memcpy(g_p_client_buffers[i].m_p_buffer, p_data_buff, buff_size);
            g_p_client_buffers[i].m_buffersize = buff_size;
            break;
        }
    }
}

int client_array_complete(int client_count)
{
    int b_complete = 1;

    for (size_t i = 0; i < g_client_buff_size; i++)
    {
        printf("%d: %p %lu\n", g_p_client_buffers[i].m_fd, g_p_client_buffers[i].m_p_buffer, g_p_client_buffers[i].m_buffersize);
        b_complete = b_complete && ((g_p_client_buffers[i].m_fd != 0) && g_p_client_buffers[i].m_p_buffer && (g_p_client_buffers[i].m_buffersize == DATA_ARRAY_SIZE(client_count - 1)));
    }

    return b_complete;
}

int buffers_equal(uint8_t* p_buffA, uint8_t* p_buffB, size_t buffASize, size_t buffBSize)
{
    if (buffASize != buffBSize)
    {
        printf("Diff on Buffer Size [%ld != %ld]\n", buffASize, buffBSize);
        return 0;
    }

    for (size_t i = 0; i < buffASize; i++)
    {
        if ((p_buffA[i] ^ p_buffB[i]) != 0)
        {
            printf("Diff on idx %ld [%X != %X]\n", i, p_buffA[i], p_buffB[i]);
            return 0;
        }
    }

    return 1;
}

int buffers_valid(int client_count)
{
    if (!g_p_data_array)
    {
        printf("Data array is NULL\n");
        return 0;
    }

    if (!client_array_complete(client_count))
    {
        printf("Client Buffers are incomplete\n");
        return 0;
    }

    for (size_t i = 0; i < g_client_buff_size; i++)
    {
        printf("Checking %d\n", g_p_client_buffers[i].m_fd);
        if (!buffers_equal(g_p_data_array, g_p_client_buffers[i].m_p_buffer, DATA_ARRAY_SIZE(client_count - 1), g_p_client_buffers[i].m_buffersize))
            return 0;
    }

    return 1;
}

int setup_seq(char* p_ip_addr, char* p_port)
{
    int port = atoi(p_port);
    int fd = create_socket(p_ip_addr, (uint16_t)port);

    if (fd < 0)
        return -1;

    if (rgcp_create_group(fd, GROUP_NAME, strlen(GROUP_NAME)) < 0)
    {
        rgcp_close(fd);
        return -1;
    }

    if (connect_to_group(fd, GROUP_NAME) < 0)
        goto error;

    rgcp_disconnect(fd);
    rgcp_close(fd);
    return 0;

error:
    rgcp_disconnect(fd);
    rgcp_close(fd);
    return -1;
}

int send_client(char* p_ip_addr, char* p_port, int client_count)
{
    printf("Starting Send Client...\n");
    int port = atoi(p_port);
    int fd = create_socket(p_ip_addr, port);

    if (fd < 0)
        return -1;

    if (connect_to_group(fd, GROUP_NAME) < 0)
    {
        rgcp_close(fd);
        return -1;
    }

    if (generate_data_array(DATA_ARRAY_SIZE(client_count - 1)) < 0)
        goto error;

    if (await_client_count(fd, client_count, WAIT_TIMEOUT_SECONDS) == 0)
        goto error;

    if (rgcp_send(fd, SENDER_MSG, strlen(SENDER_MSG), 0) < 0)
        goto error;

    if (rgcp_send(fd, g_p_data_array, DATA_ARRAY_SIZE(client_count - 1), 0) < 0)
        goto error;

    // now we can receive
    {
        rgcp_recv_data_t* p_recv_datas = NULL;
        ssize_t recv_count = 0;
        int b_complete = 0;

        do
        {
            recv_count = rgcp_recv(fd, &p_recv_datas);

            if (recv_count < 0)
                goto error;
            
            if (recv_count == 0)
                continue;

            for (ssize_t i = 0; i < recv_count; i++)
            {
                printf("%d sent %ld bytes!\n", p_recv_datas[i].m_sourceFd, p_recv_datas[i].m_bufferSize);
                load_into_client_array(p_recv_datas[i].m_sourceFd, p_recv_datas[i].m_pDataBuffer, p_recv_datas[i].m_bufferSize);
            }

            b_complete = client_array_complete(client_count);
            printf("Client array complete? %s\n", b_complete ? "True" : "False");
        } while (!b_complete);
    }

    printf("Should have received all buffers!\n");

    // check buffers here
    if (buffers_valid(client_count))
        printf("Buffers Valid\n");
    else
        printf("Buffers Not Valid\n");

    if (await_client_count(fd, 1, WAIT_TIMEOUT_SECONDS) == 0)
        goto error;

    printf("Disconnecting...\n");

    free(g_p_data_array);
    rgcp_disconnect(fd);
    rgcp_close(fd);
    return 0;

error:
    if (g_p_data_array)
        free(g_p_data_array);

    rgcp_disconnect(fd);
    rgcp_close(fd);
    return -1;
}

void* concurrent_recv_thread(void* param)
{
    int fd = *(int*)param;

    ssize_t recv_count = 0;
    do 
    {
        rgcp_recv_data_t* pRecvDatas = NULL;
        recv_count = rgcp_recv(fd, &pRecvDatas);
        rgcp_free_recv_data(pRecvDatas, recv_count);

        printf("\t%ld\n", recv_count);
    } while (recv_count >= 0 && !g_b_recv_done);

    return NULL;
}

int recv_client(char* p_ip_addr, char* p_port, int client_count)
{
    printf("Starting Recv Client...\n");
    int port = atoi(p_port);
    int fd = create_socket(p_ip_addr, port);

    if (fd < 0)
        return -1;

    if (connect_to_group(fd, GROUP_NAME) < 0)
    {
        rgcp_close(fd);
        return -1;
    }

    if (await_client_count(fd, client_count, WAIT_TIMEOUT_SECONDS) == 0)
        goto error;

    printf("All clients active\n");

    {
        rgcp_recv_data_t* p_recv_datas = NULL;
        ssize_t recv_count = 0;
        int b_send_found = 0;
        do
        {
            recv_count = rgcp_recv(fd, &p_recv_datas);
            if (recv_count < 0)
                goto error;

            if (recv_count == 0)
                continue;

            for (ssize_t i = 0; i < recv_count; i++)
            {
                if (strncmp((char*)p_recv_datas[i].m_pDataBuffer, SENDER_MSG, p_recv_datas[i].m_bufferSize) == 0)
                {
                    b_send_found = 1;
                    g_send_fd = p_recv_datas[i].m_sourceFd;
                }
            }

            rgcp_free_recv_data(p_recv_datas, recv_count);
        } while(recv_count >= 0 && !b_send_found);
    }

    printf("Sender is: %d\n", g_send_fd);

    {
        rgcp_recv_data_t* p_recv_datas = NULL;
        ssize_t recv_count = 0;
        ssize_t data_array_size = 0;

        do
        {
            recv_count = rgcp_recv(fd, &p_recv_datas);

            if (recv_count < 0)
                goto error;
            
            if (recv_count == 0)
                continue;
            
            for (ssize_t i = 0; i < recv_count; i++)
            {
                if (p_recv_datas[i].m_sourceFd != g_send_fd)
                    continue;
                
                printf("%d sent %ld bytes!\n", p_recv_datas[i].m_sourceFd, p_recv_datas[i].m_bufferSize);
                
                if (!g_p_data_array)
                {
                    g_p_data_array = calloc(p_recv_datas[i].m_bufferSize, sizeof(uint8_t));
                    memcpy(g_p_data_array, p_recv_datas[i].m_pDataBuffer, p_recv_datas[i].m_bufferSize);
                    data_array_size = p_recv_datas[i].m_bufferSize;
                }
                else
                {
                    g_p_data_array = realloc(g_p_data_array, (data_array_size + p_recv_datas[i].m_bufferSize) * sizeof(uint8_t));
                    memcpy(g_p_data_array + data_array_size, p_recv_datas[i].m_pDataBuffer, p_recv_datas[i].m_bufferSize);
                    data_array_size += p_recv_datas[i].m_bufferSize;
                }
            }
        } while(recv_count >= 0 && data_array_size != DATA_ARRAY_SIZE(client_count - 1));
    }

    printf("Received buffer @ %p (%u bytes)!\n", g_p_data_array, DATA_ARRAY_SIZE(client_count - 1));

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, concurrent_recv_thread, &fd) < 0)
        goto error;

    if (rgcp_send(fd, g_p_data_array, DATA_ARRAY_SIZE(client_count - 1), 0) < 0)
        goto error;

    printf("Sent buffer\n");
    g_b_recv_done = 1;

    if (pthread_join(recv_thread, NULL) < 0)
        goto error;

    free(g_p_data_array);
    rgcp_disconnect(fd);
    rgcp_close(fd);

    printf("Exit OK\n");
    return 0;

error:
    if (g_p_data_array)
        free(g_p_data_array);

    rgcp_disconnect(fd);
    rgcp_close(fd);
    return -1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return -1;        

    if (strcmp(argv[1], "setup") == 0)
        return setup_seq("127.0.0.1", "8000");
    
    if (argc < 3)
        return -1;

    int client_count = atoi(argv[2]);
    int res = -1;

    g_p_client_buffers = calloc(client_count - 1, sizeof(struct client_buff_data));
    g_client_buff_size = client_count - 1;

    if (strcmp(argv[1], "send") == 0)
        res = send_client("127.0.0.1", "8000", client_count);

    if (strcmp(argv[1], "recv") == 0)
        res = recv_client("127.0.0.1", "8000", client_count);

    free(g_p_client_buffers);
    return res;
}
