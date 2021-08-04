#include "generate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
void error(char *message)
{
    printf("%s\n", message);
    fflush(stdout);
    exit(0);
}
void help()
{
    printf("<No Flag> Source File\n");
    printf("-S: Output Assembly\n");
    printf("-r: Output LLVM IR\n");
    printf("-o <file>: Output file\n");
    printf("-h: Display command line information\n");
    exit(0);
}

int main(int argc, char **argv)
{
    FILE *fp;
    // Find LLVM directory
    fp = popen("/bin/llvm-config --bindir", "r");
    if (fp == NULL)
    {
        printf("Failed to get bin directory\n");
        exit(1);
    }

    // Get LLVM bin and remove newline
    char path[1024];
    fgets(path, 1024, fp);
    path[strlen(path)-1] = 0;

    char *output="a.o";
    char *flags;

    //boolean for flags
    int scheck = 0;
    int ocheck = 0;
    int rcheck = 0;

    int opt;
    //getopt parses command line arguments
    while ((opt = getopt(argc, argv, "So:hr")) != -1)
    {
        switch (opt)
        {
        case 'S':
            scheck = 1;
            break;
        case 'o':
            ocheck = 1;
            output= strdup(optarg);
            break;
        case 'h':
            help();
            break;
        case 'r':
            rcheck=1;
            break;
        default:
            printf("Invalid Command. Use the following commands:");
            help();
        }
    }
    pclose(fp);
    if(optind >= argc || argv[optind]==NULL || (scheck & rcheck)){
        help();
    }
    printf("%s\n", path);
    generate(path, argv[optind], output, scheck, rcheck);
}