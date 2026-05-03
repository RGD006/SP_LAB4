#include "filesystem.h"
#include "block.h"
#include "directory.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ARG (16) // max number of arguments to parse

static dir_t *root = NULL;
static size_t id = 0;

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

// TODO: make equal name fallback
static int8_t __create_file(const char *name) {
    if (!name) 
        return -1;

    id++;
    inode_t *newi = in_create(id, 0);
    hard_link_t *newh = hl_create(name, newi);
    dir_add_hl(root, newh);

    return 0;
}

// TODO: add show directory by path
static int8_t __show_directory(void) {
    if (!root)
        return -1;

    hard_link_t *step = root->head;

    if (!step)
        return -1;

    while (step != NULL) {
        printf("file[%lu]\t<\t%lu,\t%lu,\t%lu>:\t%s\n", 
            step->meta->id,
            step->meta->file_size,
            step->meta->h_links_number,
            step->meta->create_time,
            step->name
        );

        step = step->next;
    }

    return 0;
}

static int8_t __stat_file(char *path) {
    if (!root)
        return -1;

    hard_link_t *step = root->head;

    if (!step)
        return -1;

    while (step != NULL) { 
        if (strcmp(path, step->name) == 0) {
            printf("file[%lu]\t<\t%lu,\t%lu,\t%lu>:\t%s\n", 
                step->meta->id,
                step->meta->file_size,
                step->meta->h_links_number,
                step->meta->create_time,
                step->name
            );   

            return 0;
        }
        step = step->next;
    }

    return -2;
}

int8_t fs_parse_command(char *comm_line) {
    if (!root)
        root = dir_create(id);

    char **arguments = __split_string(comm_line, " \t\n\r");

    if (!arguments)
        return -1;

    // DEBUG PARSER
    // while (arguments[i] != NULL) {
    //     printf("argument[%lu]: %s %lu | ", i, arguments[i], strlen(arguments[i]));
    //     for (size_t j = 0; j < strlen(arguments[i]); j++)
    //         printf("%02X ", arguments[i][j]);
    //     printf("\n");
    //     i++;
    // }
    char *command = arguments[0];

    if (strcmp(command, "quit") == 0) {
        return 0;
    } else if (strcmp(command, "create") == 0) {
        if (arguments[1])
            __create_file(arguments[1]);
    } else if (strcmp(command, "ls") == 0) {
        __show_directory();
    } else if (strcmp(command, "stat") == 0) {
        if (arguments[1]) {
            int8_t result = __stat_file(arguments[1]);

            if (result == -2) {
                printf("Can't find file %s\n", arguments[1]);
                return -1;
            }
        } 

        return -1;
    }
    

    return INT8_MAX;
}
