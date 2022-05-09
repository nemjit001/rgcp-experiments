#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <rgcp/rgcp.h>

int main(int argc, char** argv)
{
    if (argc < 2)
        return -1;

    if (argc == 5)
        printf("%s\tRGCP THROUGHPUT\n", argv[4]);
    else
        printf("---\tRGCP THROUGHPUT\t---\n");

    if (strcmp(argv[1], "recv") == 0)
        return 0;

    if (strcmp(argv[1], "send") == 0)
        return 0;

    return -1;
}