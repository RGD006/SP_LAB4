#include "filesystem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ARG (16) // max number of arguments to parse

static char **__split_string(char *string, const char *delimeter) {
    if (!string || strlen(string) == 0)
        return NULL;

    char **result = calloc(MAX_ARG, sizeof(char *));
    char *token = strtok(string, delimeter);
    uint32_t n_slices = 0;

    if (!token || !result)
        return NULL;

    while (token != NULL) {
        result[n_slices] = calloc(strlen(token), sizeof(char));
        if (!result[n_slices])
            return NULL;
        result[n_slices] = token;
        n_slices++;
        token = strtok(NULL, delimeter);
    }

    if (result == NULL)
        return NULL;
    
    result[n_slices] = NULL; // end of splitted string
    return result;
}

int8_t fs_parse_command(char *comm_line) {
    char **arguments = __split_string(comm_line, " \t\n\r");

    if (!arguments)
        return -1;

    size_t i = 0;

    // DEBUG PARSER
    // while (arguments[i] != NULL) {
    //     printf("argument[%lu]: %s %lu | ", i, arguments[i], strlen(arguments[i]));
    //     for (size_t j = 0; j < strlen(arguments[i]); j++)
    //         printf("%02X ", arguments[i][j]);
    //     printf("\n");
    //     i++;
    // }

    if (strcmp(arguments[0], "quit") == 0)
        return 0;

    return INT8_MAX;
}
