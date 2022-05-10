#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <rgcp/rgcp.h>

int data_send(char* middleware_ip, char* middleware_group)
{
    return 0;
}

int data_recv(char* middleware_ip, char* middleware_group)
{
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