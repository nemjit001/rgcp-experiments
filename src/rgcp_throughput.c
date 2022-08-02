#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <rgcp/rgcp.h>

#define KiB_SIZE(kb) (kb << 10)
#define MiB_SIZE(mb) (KiB_SIZE(mb) << 10)
#define GiB_SIZE(gb) (MiB_SIZE(gb) << 10)

// #define DEBUG_PRINTS

#define GROUP_NAME "RGCP_TP_TEST"
#define SENDER_MSG "SND"
#define END_MSG "END"

#define WAIT_TIMEOUT_SECONDS 10
#define DATA_ARRAY_START_TOKEN 0xAA
#define DATA_ARRAY_SIZE(peer_count) (uint32_t)((uint32_t)GiB_SIZE(1) / peer_count)

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
        #ifdef DEBUG_PRINTS
            printf("%d: %p %lu\n", g_p_client_buffers[i].m_fd, g_p_client_buffers[i].m_p_buffer, g_p_client_buffers[i].m_buffersize);
        #endif
        b_complete = b_complete && ((g_p_client_buffers[i].m_fd != 0) && g_p_client_buffers[i].m_p_buffer && (g_p_client_buffers[i].m_buffersize == DATA_ARRAY_SIZE(client_count - 1)));
    }

    return b_complete;
}

int buffers_equal(uint8_t* p_buffA, uint8_t* p_buffB, size_t buffASize, size_t buffBSize)
{
    if (buffASize != buffBSize)
    {
        printf("\tDiff on Buffer Size [%ld != %ld]\n", buffASize, buffBSize);
        return 0;
    }

    for (size_t i = 0; i < buffASize; i++)
    {
        if ((p_buffA[i] ^ p_buffB[i]) != 0)
        {
            printf("\tDiff on idx %ld [%X %X %X] vs [%X %X %X]\n",
                i,
                (i > 0) ? p_buffA[i - 1] : 0x0,
                p_buffA[i],
                (i < buffASize - 1) ? p_buffA[i + 1] : 0x0,
                (i > 0) ? p_buffB[i - 1] : 0x0,
                p_buffB[i],
                (i < buffASize - 1) ? p_buffB[i + 1] : 0x0
            );
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
        #ifdef DEBUG_PRINTS
            printf("Checking %d\n", g_p_client_buffers[i].m_fd);
        #endif
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
    printf("RGCP TP SEND\n");
    int port = atoi(p_port);
    int fd = create_socket(p_ip_addr, port);

    if (fd < 0)
    {
        printf("failed to create socket\n");
        return -1;
    }

    if (connect_to_group(fd, GROUP_NAME) < 0)
    {
        printf("failed to connect to group\n");
        rgcp_close(fd);
        return -1;
    }

    if (generate_data_array(DATA_ARRAY_SIZE(client_count - 1)) < 0)
    {
        printf("failed to generate array\n");
        goto error;
    }

    if (await_client_count(fd, client_count, WAIT_TIMEOUT_SECONDS) == 0)
    {
        printf("not enough clients\n");
        goto error;
    }

    if (rgcp_send(fd, SENDER_MSG, strlen(SENDER_MSG), 0, NULL) < 0)
    {
        printf("SND fail\n");
        goto error;
    }

    struct timespec start_time, end_time;

    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    if (rgcp_send(fd, g_p_data_array, DATA_ARRAY_SIZE(client_count - 1), 0, NULL) < 0)
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
            {
                printf("recv_count < 0\n");
                goto error;
            }
            
            if (recv_count == 0)
                continue;

            for (ssize_t i = 0; i < recv_count; i++)
            {
                #ifdef DEBUG_PRINTS
                    printf("%d sent %ld bytes!\n", p_recv_datas[i].m_sourceFd, p_recv_datas[i].m_bufferSize);
                #endif
                load_into_client_array(p_recv_datas[i].m_sourceFd, p_recv_datas[i].m_pDataBuffer, p_recv_datas[i].m_bufferSize);
            }
        
            b_complete = client_array_complete(client_count);
            #ifdef DEBUG_PRINTS
                printf("Client array complete? %s\n", b_complete ? "True" : "False");
            #endif
        } while (!b_complete);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    #ifdef DEBUG_PRINTS
        printf("Should have received all buffers!\n");
    #endif
    
    struct timespec total_time = get_delta(start_time, end_time);
    ssize_t seconds = total_time.tv_sec;
    ssize_t ms = total_time.tv_nsec / 1000000;

    // check buffers here
    if (buffers_valid(client_count))
        printf("Buffers Valid @ %lus:%lums\n", seconds, ms);
    else
        printf("Buffers Not Valid\n");

    if (rgcp_send(fd, END_MSG, strlen(END_MSG), 0, NULL) < 0)
        goto error;

    if (await_client_count(fd, 1, WAIT_TIMEOUT_SECONDS) == 0)
        goto error;

    #ifdef DEBUG_PRINTS
        printf("Disconnecting...\n");
    #endif

    free(g_p_data_array);
    rgcp_disconnect(fd);
    rgcp_close(fd);
    return 0;

error:
    #ifdef DEBUG_PRINTS
        printf("Error\n");
    #endif

    if (g_p_data_array)
        free(g_p_data_array);

    rgcp_disconnect(fd);
    rgcp_close(fd);
    return -1;
}

int recv_client(char* p_ip_addr, char* p_port, int client_count)
{
    printf("RGCP TP RECV\n");
    int port = atoi(p_port);
    int fd = create_socket(p_ip_addr, port);
    rgcp_unicast_mask_t send_mask;
    send_mask.m_targetFd = -1;

    if (fd < 0)
        return -1;

    if (connect_to_group(fd, GROUP_NAME) < 0)
    {
        rgcp_close(fd);
        return -1;
    }

    if (await_client_count(fd, client_count, WAIT_TIMEOUT_SECONDS) == 0)
        goto error;

    #ifdef DEBUG_PRINTS
        printf("All clients active\n");
    #endif

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

    #ifdef DEBUG_PRINTS
        printf("Sender is: %d\n", g_send_fd);
    #endif

    send_mask.m_targetFd = g_send_fd;

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
                
                #ifdef DEBUG_PRINTS
                    printf("%d sent %ld bytes!\n", p_recv_datas[i].m_sourceFd, p_recv_datas[i].m_bufferSize);
                #endif
                
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

    #ifdef DEBUG_PRINTS
        printf("Received buffer @ %p (%u bytes)!\n", g_p_data_array, DATA_ARRAY_SIZE(client_count - 1));
    #endif

    if (rgcp_send(fd, g_p_data_array, DATA_ARRAY_SIZE(client_count - 1), RGCP_SEND_UNICAST, &send_mask) < 0)
        goto error;

    #ifdef DEBUG_PRINTS
        printf("Sent buffer\n");
    #endif

    g_b_recv_done = 1;

    {
        rgcp_recv_data_t* p_recv_datas = NULL;
        ssize_t recv_count = 0;
        int b_end_found = 0;
        do
        {
            recv_count = rgcp_recv(fd, &p_recv_datas);
            if (recv_count < 0)
                goto error;

            if (recv_count == 0)
                continue;

            for (ssize_t i = 0; i < recv_count; i++)
            {
                if (strncmp((char*)p_recv_datas[i].m_pDataBuffer, END_MSG, p_recv_datas[i].m_bufferSize) == 0)
                    b_end_found = 1;
            }

            rgcp_free_recv_data(p_recv_datas, recv_count);
        } while(recv_count >= 0 && !b_end_found);
    }

    free(g_p_data_array);
    rgcp_disconnect(fd);
    rgcp_close(fd);

    printf("Exit OK\n");
    return 0;

error:
    printf("Error\n");

    if (g_p_data_array)
        free(g_p_data_array);

    rgcp_disconnect(fd);
    rgcp_close(fd);
    return -1;
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return -1;

    int client_count = atoi(argv[2]);

    char* ip_addr = "127.0.0.1";
    char* port = "8000";
    int res = -1;

    if (argc >= 4)
        ip_addr = argv[3];

    if (argc >= 5)
        port = argv[4];

    if (strcmp(argv[1], "setup") == 0)
        return setup_seq(ip_addr, port);

    g_p_client_buffers = calloc(client_count - 1, sizeof(struct client_buff_data));
    g_client_buff_size = client_count - 1;

    if (strcmp(argv[1], "send") == 0)
        res = send_client(ip_addr, port, client_count);

    if (strcmp(argv[1], "recv") == 0)
        res = recv_client(ip_addr, port, client_count);

    free(g_p_client_buffers);
    return res;
}
