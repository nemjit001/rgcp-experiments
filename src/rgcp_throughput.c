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
#define DATA_ARRAY_SIZE MiB_SIZE(1)

static int remote_count = 0;
static uint8_t* data_array = NULL;

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

    ssize_t send_res = rgcp_send(fd, (char*)data_array, DATA_ARRAY_SIZE, 0);
    printf("sent %ld bytes\n", send_res);

    if (send_res < 0)
        goto error;

    // keep receiving until remotes stop sending

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

    sleep(remote_count);

    printf("%ld\n", rgcp_peer_count(fd));

    // keep receiving until remote stops sending
    // assert that the buffer is correct size
    // echo bytes back to everyone else in the group

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