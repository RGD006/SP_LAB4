#include "filesystem.h"
#include "block.h"
#include "directory.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ARG (16) // max number of arguments to parse
#define MAX_FD  (128) // max nummer of file descriptors

static dir_t *root = NULL;
static hard_link_t *fd[MAX_FD] = {0};
static size_t id = 0;

static int8_t __open_fd(const char *filepath) {
    hard_link_t *step = root->head;
    uint8_t file_find = 0;

    if (!step) 
        return -1;

    // find file in dir
    while (step != NULL) {
        if (strcmp(step->name, filepath) == 0) {
            step->meta->flags.is_open = 1;
            file_find = 1;
            break;
        }

        step = step->next;
    }

    if (!file_find) 
        return -2; // can't find file in directory

    // get fd from fd list
    for (uint8_t i = 0; i < MAX_FD; i++) {
        if (!fd[i]) {
            fd[i] = step;
            return i;
        }
    }

    return -3; // all fd is reserved
}

static int8_t __close_fd(const unsigned long selected_fd) {
    if (!fd[selected_fd] || !fd[selected_fd]->meta) {
        return -1;
    }

    fd[selected_fd] = NULL;
    fd[selected_fd]->meta->offset = 0;

    return 0;
}

static int8_t __seek_fd(const uint8_t selected_fd, const uint64_t offset) {
    if (!fd[selected_fd] && fd[selected_fd]->meta)
        return -1;

    inode_t *selected_inode = fd[selected_fd]->meta;

    if (!selected_inode)
        return -2;

    // Handle if offset is bigger, than file size
    if (selected_inode->file_size < offset) {
        size_t create_block = (offset - selected_inode->file_size) / MAX_B;
        selected_inode->file_size = create_block * MAX_B;
        block_t *step = selected_inode->head;

        if (!step)
            return -3;

        // Create new data block
        for (size_t i = create_block; i > 0; i--) {
            step->next = bl_create(step, NULL);
            step = step->next;
        }
    }

    selected_inode->offset = offset;

    return 0;    
}

static int8_t __write_fd(const uint8_t selected_fd) {
    return 0;
}

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
            printf("file[%lu]\t<\t%lu,\t%lu,\t%lu>:\t%s\noffset: %lu\n", 
                step->meta->id,
                step->meta->file_size,
                step->meta->h_links_number,
                step->meta->create_time,
                step->name,
                step->meta->offset
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

    switch (command[0]) {
        case 'q':
            return INT8_MAX;
        case 'c':
            switch (command[1]) {
                case 'r':
                    if (!arguments[1]) {
                        printf("Enter arguments\n");
                        return -1;
                    }
                    
                    __create_file(arguments[1]);
                    return 0;
                case 'l':
                    if (!arguments[1]) {
                        printf("Enter arguments\n");
                        return -1;
                    }

                    char *endptr = NULL;
                    unsigned long converted = strtoul(
                        arguments[1],
                        &endptr,
                        5
                    );

                    if (endptr == arguments[1] || *endptr != '\0') {
                        printf("Enter digit to close FD\n");
                        return -1;
                    } else if (__close_fd(converted) == 0) {
                        // FD success close
                        printf("FD %lu is closed\n", converted);
                        return 0;
                    }

                    return -1;
            }
            return -1;
        case 'l':
            __show_directory();
            return 0;
        case 's':
            switch (command[1]) {
                case 't':
                    if (!arguments[1]) {
                        printf("Enter arguments\n");
                        return -1;
                    } 

                    int8_t result = __stat_file(arguments[1]);

                    if (result == -2) {
                        printf("Can't find file %s\n", arguments[1]);
                        return -1;
                    }

                    return 0;
                case 'e': {
                    char *endptr = NULL;
                    unsigned int cfd, coffset;
                    if (!arguments[1] || !arguments[2]) {
                        printf("Enter fd and offset");
                        return -1;
                    }
                    
                    cfd = strtoul(
                        arguments[1],
                        &endptr,
                        0
                    );

                    if (endptr == arguments[1] || *endptr != '\0') {
                        printf("Enter FD\n");
                        return -1;
                    }

                    endptr = NULL;

                    coffset = strtoul(
                        arguments[2],
                        &endptr,
                        0
                    );

                    if (endptr == arguments[2] || *endptr != '\0') {
                        printf("Enter offset\n");
                        return -1;
                    }

                    int8_t ret = __seek_fd(cfd, coffset);

                    switch (ret) {
                        case -1:
                            printf("Wrong FD\n");
                            return -1;
                        case -2:
                            printf("Uninit inode\n");
                            return -1;
                        case -3:
                            printf("Block error\n");
                            return -1;
                        default:
                            printf("Set offset\n");
                            return 0;
                    }
                }
            }
            return -1;
        case 'o':
            if (!arguments[1]) {
                printf("Enter filepath\n");
                return -1;
            }

            int8_t rfd = __open_fd(arguments[1]);

            switch (rfd) {
                case -1:
                    printf("FS is not init\n");
                    return -1;
                case -2:
                    printf("Can't find file in path\n");
                    return -1;
                case -3:
                    printf("All FD is reserved\n");
                    return -1;
            }

            printf("Create FD for %s: %d\n", arguments[1], rfd);
            return 0;
    }

    return INT8_MAX - 1;
}
