#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <rgcp/rgcp.h>

int create_socket(char* ip)
{
    struct sockaddr_in mw_addr;
    mw_addr.sin_addr.s_addr = inet_addr(ip);
    mw_addr.sin_family = AF_INET;
    mw_addr.sin_port = htons(8000);
    return rgcp_socket(AF_INET, (struct sockaddr*)&mw_addr, sizeof(mw_addr));
}

int data_send(char* middleware_ip, char* middleware_group)
{
    int fd = create_socket(middleware_ip);

    if (fd < 0)
        goto error;

    rgcp_group_info_t** group_infos = NULL;
    ssize_t group_count = rgcp_discover_groups(fd, &group_infos);

    if (group_count < 0)
        goto error;

    rgcp_group_info_t* target_group = NULL;
    for (ssize_t i = 0; i < group_count; i++)
    {
        if (strcmp(middleware_group, group_infos[i]->m_pGroupName) == 0)
        {
            if (target_group && strcmp(target_group->m_pGroupName, group_infos[i]->m_pGroupName) == 0 && target_group->m_groupNameHash > group_infos[i]->m_groupNameHash)
                continue;
            else
                target_group = group_infos[i];
        }
    }

    if (rgcp_connect(fd, *target_group) < 0)
        goto error;
        
    //

    rgcp_disconnect(fd);
    rgcp_close(fd);

    return 0;

error:
    perror(NULL);

    rgcp_disconnect(fd);
    rgcp_close(fd);

    return -1;
}

int data_recv(char* middleware_ip, char* middleware_group)
{
    //int fd = create_socket(middleware_ip);

    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 4)
        return -1;

    if (argc == 5)
        printf("%s\tRGCP THROUGHPUT\n", argv[4]);
    else
        printf("---\tRGCP THROUGHPUT\t---\n");

    if (strcmp(argv[1], "recv") == 0)
        return data_recv(argv[2], argv[3]);

    if (strcmp(argv[1], "send") == 0)
        return data_send(argv[2], argv[3]);

    return -1;
}