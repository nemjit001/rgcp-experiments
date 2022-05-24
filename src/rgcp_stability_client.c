#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <rgcp/rgcp.h>

static int duration_seconds = 0;
static int rand_seed = 0;
static int disconnect_chance = 10;
static char* p_group_name = "STAB_GROUP_NONUM";
static int b_is_connected = 0;

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

int create_socket(char* ip)
{
    struct sockaddr_in mw_addr;
    mw_addr.sin_addr.s_addr = inet_addr(ip);
    mw_addr.sin_family = AF_INET;
    mw_addr.sin_port = htons(8000);
    return rgcp_socket(AF_INET, (struct sockaddr*)&mw_addr, sizeof(mw_addr));
}

rgcp_group_info_t* get_or_create_group(int rgcp_fd, char* group_name)
{
    rgcp_group_info_t* p_req_group = NULL;
    rgcp_group_info_t** pp_group_infos = NULL;
    ssize_t group_count = rgcp_discover_groups(rgcp_fd, &pp_group_infos);

    for (ssize_t i = 0; i < group_count; i++)
    {
        if (!p_req_group && strcmp(group_name, pp_group_infos[i]->m_pGroupName) == 0)
        {
            p_req_group = calloc(1, sizeof(rgcp_group_info_t));
            p_req_group->m_groupNameHash = pp_group_infos[i]->m_groupNameHash;
            p_req_group->m_groupNameLength = pp_group_infos[i]->m_groupNameLength;
            p_req_group->m_pGroupName = calloc(p_req_group->m_groupNameLength, sizeof(char));
            memcpy(p_req_group->m_pGroupName, pp_group_infos[i]->m_pGroupName, p_req_group->m_groupNameLength);
        }
        else if (strcmp(group_name, pp_group_infos[i]->m_pGroupName) == 0 && p_req_group->m_groupNameHash < pp_group_infos[i]->m_groupNameHash)
        {
            p_req_group->m_groupNameHash = pp_group_infos[i]->m_groupNameHash;
            p_req_group->m_groupNameLength = pp_group_infos[i]->m_groupNameLength;

            free(p_req_group->m_pGroupName);
            p_req_group->m_pGroupName = calloc(p_req_group->m_groupNameLength, sizeof(char));
            memcpy(p_req_group->m_pGroupName, pp_group_infos[i]->m_pGroupName, p_req_group->m_groupNameLength);
        }
    }

    rgcp_free_group_infos(&pp_group_infos, group_count);

    if (p_req_group == NULL)
    {
        rgcp_create_group(rgcp_fd, group_name, strlen(group_name));
        return get_or_create_group(rgcp_fd, group_name);
    }

    return p_req_group;
}

int main(int argc, char** argv)
{
    if (argc >= 2)
        duration_seconds = atoi(argv[1]);

    if (argc >= 3)
        disconnect_chance = atoi(argv[2]);

    if (argc >= 4)
        rand_seed = atoi(argv[3]);

    if (argc >= 5)
        p_group_name = argv[4];

    if (rand_seed == 0)
        srand((unsigned)time(NULL));
    else
        srand(rand_seed);
    

    int fd = create_socket("127.0.0.1");

    rgcp_group_info_t* p_group_info = get_or_create_group(fd, p_group_name);

    if (!p_group_info)
    {
        rgcp_close(fd);
        return -1;
    }

    struct timespec start_time, curr_lap, total_time;
    total_time.tv_sec = 0;
    total_time.tv_nsec = 0;

    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    do
    {
        if (rand() % disconnect_chance == 0)
        {
            if (b_is_connected)
            {
                if (rgcp_disconnect(fd) < 0)
                    break;
            }
            else
            {
                if (rgcp_connect(fd, *p_group_info) < 0)
                    break;
            }

            b_is_connected = !b_is_connected;
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &curr_lap);
        total_time = get_delta(start_time, curr_lap);
    } while (total_time.tv_sec < duration_seconds);   

    printf("Stability test done in %lds:%ldms\n", total_time.tv_sec, total_time.tv_nsec / 100000);

    if (b_is_connected)
        if (rgcp_disconnect(fd) < 0)
            printf("Error in Disconnect");

    free(p_group_info->m_pGroupName);
    free(p_group_info);
    if (rgcp_close(fd) < 0)
        printf("Error in Close\n");

    return 0;
}