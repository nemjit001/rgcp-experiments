#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <rgcp/rgcp.h>

#define KiB_SIZE(kb) (kb << 10)
#define MiB_SIZE(mb) (KiB_SIZE(mb) << 10)
#define GiB_SIZE(gb) (MiB_SIZE(gb) << 10)

#define GROUP_NAME "RGCP_TP_TEST"
#define SENDER_MSG "SND"

#define WAIT_TIMEOUT_SECONDS 10
#define DATA_ARRAY_START_TOKEN 0xAA
#define DATA_ARRAY_SIZE 1024

static uint8_t* g_p_data_array = NULL;
static int g_send_fd = -1;

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

    if (generate_data_array(DATA_ARRAY_SIZE) < 0)
        goto error;

    if (await_client_count(fd, client_count, WAIT_TIMEOUT_SECONDS) == 0)
        goto error;

    // let peers know this is the sender
    if (rgcp_send(fd, SENDER_MSG, strlen(SENDER_MSG), 0) < 0)
        goto error;

    // send array to peers
    if (rgcp_send(fd, g_p_data_array, DATA_ARRAY_SIZE, 0) < 0)
        goto error;

    // now we can receive

    // send OK to peers

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

    // now we can receive the sender
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
        // now we can keep reading from sender

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
            }
        } while(recv_count >= 0);
    }

    // now we can send

    rgcp_disconnect(fd);
    rgcp_close(fd);
    return 0;

error:
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

    if (strcmp(argv[1], "send") == 0)
        res = send_client("127.0.0.1", "8000", client_count);

    if (strcmp(argv[1], "recv") == 0)
        res = recv_client("127.0.0.1", "8000", client_count);

    return res;
}
