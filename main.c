#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "rfilesystem/filesystem.h"


int main(int argc, char **argv) {
    char command[MAX_C];

    while (1) {
        printf("write command:\n");
        if (fgets(command, MAX_C, stdin) == NULL) {
            if (feof(stdin))
                break;
        }

        if (fs_parse_command(command) == 0) 
            break;

        memset(command, 0, MAX_C);
    }

    return 0;
}