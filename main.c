#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "rfilesystem/filesystem.h"


int main() {
    char command[MAX_C];

    while (1) {
        printf("write command:\n");
        if (fgets(command, MAX_C, stdin) == NULL) {
            if (feof(stdin))
                break;
        }

        if (fs_parse_command(command) == INT8_MAX) 
            break;

        memset(command, 0, MAX_C);
    }

    return 0;
}