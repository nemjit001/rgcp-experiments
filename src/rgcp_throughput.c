#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <rgcp/rgcp.h>

#define KiB_SIZE(kb) (kb << 10)
#define MiB_SIZE(mb) (KiB_SIZE(mb) << 10)
#define GiB_SIZE(gb) (MiB_SIZE(gb) << 10)

//#define DEBUG_PRINTS
#define START_TOKEN 0xAA
#define DATA_ARRAY_SIZE MiB_SIZE(10)

static int remote_count = 0;
static uint8_t* data_array = NULL;

struct recv_info
{
    int m_sourceFd;
    uint8_t* m_pBuffer;
    size_t m_bufferSize;
};

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

struct timespec get_delta(struct timespec A, struct timespec B)
{
    ssize_t seconds = B.tv_sec - A.tv_sec;
    ssize_t ns = (B.tv_nsec - A.tv_nsec);

    if (ns < 0)
    {
        ns += 1000000000;
        seconds--;
    }

    struct timespec res;
    res.tv_nsec = ns;
    res.tv_sec = seconds;
    return res;
}

int create_socket(char* ip)
{
    struct sockaddr_in mw_addr;
    mw_addr.sin_addr.s_addr = inet_addr(ip);
    mw_addr.sin_family = AF_INET;
    mw_addr.sin_port = htons(8000);
    return rgcp_socket(AF_INET, (struct sockaddr*)&mw_addr, sizeof(mw_addr));
}

int connect_to_grp(int fd, char* groupname)
{
    rgcp_group_info_t** group_infos = NULL;
    ssize_t group_count = rgcp_discover_groups(fd, &group_infos);

    if (group_count < 0)
        return -1;

    rgcp_group_info_t* target_group = NULL;
    for (ssize_t i = 0; i < group_count; i++)
    {
        if (strcmp(groupname, group_infos[i]->m_pGroupName) == 0)
        {
            if (target_group && strcmp(target_group->m_pGroupName, group_infos[i]->m_pGroupName) == 0 && target_group->m_groupNameHash > group_infos[i]->m_groupNameHash)
                continue;
            else
                target_group = group_infos[i];
        }
    }

    if (target_group == NULL)
        return 0;
    
    if (rgcp_connect(fd, *target_group) < 0)
    {
        rgcp_free_group_infos(&group_infos, group_count);
        return -1;
    }

    rgcp_free_group_infos(&group_infos, group_count);
    return 1;
}

void await_remote_connections(int fd, int expected_count)
{
    while (rgcp_peer_count(fd) != expected_count)
    {
        usleep(5000);
    }
}

int data_send(char* middleware_ip, char* middleware_group)
{
    if (generate_data_array(DATA_ARRAY_SIZE) < 0)
        return -1;

    int fd = create_socket(middleware_ip);

    if (fd < 0)
        goto error;

    printf("connecting...\n");

    int connect_res = connect_to_grp(fd, middleware_group);
    while (connect_res == 0)
    {
        rgcp_create_group(fd, middleware_group, strlen(middleware_group));
        connect_res = connect_to_grp(fd, middleware_group);

        if (connect_res < 0)
            goto error;
    }  
        
    printf("connected to group!\n");
    
    await_remote_connections(fd, remote_count);

    printf("Remotes ready\n");

    struct timespec start_rtt;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_rtt);
    ssize_t send_res = rgcp_send(fd, data_array, DATA_ARRAY_SIZE, 0);

    if (send_res < 0)
        goto error;

    printf("Send done\n");

    struct recv_info* p_recv_infos = calloc(remote_count, sizeof(struct recv_info));
    size_t recv_info_idx = 0;
    ssize_t recv_count = 0;

    printf("Starting recv\n");

    do
    {
        rgcp_recv_data_t *p_recv_data = NULL;
        recv_count = rgcp_recv(fd, &p_recv_data);

        if (recv_count > 0)
            printf("Recvd %ld buffers\n", recv_count);

        for (ssize_t i = 0; i < recv_count; i++)
        {
            printf("(%d %lu)\n", p_recv_data[i].m_sourceFd, p_recv_data[i].m_bufferSize);

            p_recv_infos[recv_info_idx].m_sourceFd = p_recv_data[i].m_sourceFd;
            p_recv_infos[recv_info_idx].m_pBuffer = calloc(p_recv_data[i].m_bufferSize, sizeof(uint8_t));
            p_recv_infos[recv_info_idx].m_bufferSize = p_recv_data[i].m_bufferSize;
            memcpy(p_recv_infos[recv_info_idx].m_pBuffer, p_recv_data[i].m_pDataBuffer, p_recv_data[i].m_bufferSize);
            recv_info_idx++;
        }
        
        if (recv_count > 0)
        {
            rgcp_free_recv_data(p_recv_data, recv_count);
        }

    } while(recv_info_idx < remote_count);

    struct timespec end_rtt;
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_rtt);

    printf("Validating Buffers\n");
    int bBuffersValid = 1;

    for (int i = 0; i < remote_count; i++)
    {
        if (validate_buffers(data_array, p_recv_infos[i].m_pBuffer, DATA_ARRAY_SIZE, p_recv_infos[i].m_bufferSize) == 0)
        {
            printf("Buffer %d Invalid\n", i);
            bBuffersValid = 0;
        }

        free(p_recv_infos[i].m_pBuffer);
    }

    if (!bBuffersValid)
    {
        rgcp_disconnect(fd);
        rgcp_close(fd);
        free(data_array);

        return -1;
    }

    struct timespec rtt = get_delta(start_rtt, end_rtt);
    printf("Buffers Valid @ %lus:%lums\n", rtt.tv_sec, rtt.tv_nsec / 1000000);

    rgcp_disconnect(fd);
    rgcp_close(fd);
    free(data_array);

    return 0;

error:
    perror(NULL);

    rgcp_disconnect(fd);
    rgcp_close(fd);
    free(data_array);

    return -1;
}

int data_recv(char* middleware_ip, char* middleware_group)
{
    int fd = create_socket(middleware_ip);

    if (fd < 0)
        goto error;

    int connect_res = connect_to_grp(fd, middleware_group);
    while (connect_res == 0)
    {
        rgcp_create_group(fd, middleware_group, strlen(middleware_group));
        connect_res = connect_to_grp(fd, middleware_group);

        if (connect_res < 0)
            goto error;
    }
        
    printf("connected to group!\n");

    await_remote_connections(fd, remote_count);

    printf("%ld\n", rgcp_peer_count(fd));

    printf("Starting recv\n");

    uint8_t *total_recv_buffer = NULL;
    size_t recv_buffer_size = 0;
    ssize_t recv_count = 0;
    do
    {
        rgcp_recv_data_t *p_recv_data = NULL;
        recv_count = rgcp_recv(fd, &p_recv_data);

        if (recv_count > 0)
            printf("Recvd %ld buffers\n", recv_count);

        rgcp_recv_data_t* p_actual_data = NULL;
        // find the highest source fd, last connected client is sender
        for (ssize_t i = 0; i < recv_count; i++)
        {
            if (p_actual_data == NULL || p_recv_data->m_sourceFd < p_recv_data[i].m_sourceFd)
                p_actual_data = &p_recv_data[i];
        }

        if (recv_count == 0)
            continue;
        
        if (total_recv_buffer == NULL)
        {
            total_recv_buffer = calloc(p_actual_data->m_bufferSize, sizeof(uint8_t));
            memcpy(total_recv_buffer, p_actual_data->m_pDataBuffer, p_actual_data->m_bufferSize);
        }
        else
        {
            uint8_t* p_temp = realloc(total_recv_buffer, (recv_buffer_size + p_actual_data->m_bufferSize) * sizeof(uint8_t));

            if (p_temp == NULL)
                goto error;

            total_recv_buffer = p_temp;
            memcpy(total_recv_buffer + recv_buffer_size, p_actual_data->m_pDataBuffer, p_actual_data->m_bufferSize);
        }

        recv_buffer_size += p_actual_data->m_bufferSize;
        rgcp_free_recv_data(p_recv_data, recv_count);
    } while(recv_buffer_size != DATA_ARRAY_SIZE);

    printf("Total recv buff size: %zu bytes\n", recv_buffer_size);

    if (recv_buffer_size != DATA_ARRAY_SIZE)
        goto error;

    // echo bytes back to everyone else in the group
    ssize_t send_res = rgcp_send(fd, total_recv_buffer, recv_buffer_size, 0);

    if (send_res < 0)
        goto error;

    printf("Echoed %ld bytes\n", send_res);

    sleep(remote_count);

    rgcp_disconnect(fd);
    rgcp_close(fd);

    return 0;

error:
    perror(NULL);

    rgcp_disconnect(fd);
    rgcp_close(fd);

    return -1;
}

int main(int argc, char** argv)
{
    if (argc < 5)
        return -1;

    remote_count = atoi(argv[4]);

    if (argc == 6)
        printf("%s\tRGCP THROUGHPUT\n", argv[5]);
    else
        printf("---\tRGCP THROUGHPUT\t---\n");

    if (strcmp(argv[1], "recv") == 0)
        return data_recv(argv[2], argv[3]);

    if (strcmp(argv[1], "send") == 0)
        return data_send(argv[2], argv[3]);

    return -1;
}