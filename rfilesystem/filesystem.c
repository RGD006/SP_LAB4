#include "filesystem.h"
#include "block.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ARG (16) // max number of arguments to parse
#define MAX_FD  (128) // max nummer of file descriptors

static dir_t *root = NULL, *user_pos = NULL;
static hard_link_t *fd[MAX_FD] = {0};
static size_t id = 0;
static hard_link_t *insert_dir(char *path);

const char *last_path_component(const char *path) {
    if (!path || *path == '\0')
        return path;

    size_t len = strlen(path);

    while (len > 1 && path[len - 1] == '/')
        len--;

    while (len > 0 && path[len - 1] != '/')
        len--;

    if (len == 0)
        return path;

    if (path[len] == '\0')
        return "/";

    return path + len;
}

static hard_link_t *__find_path(const char *path, uint8_t follow_last_symlink) {
    if (!path)
        return NULL;

    char resolved[MAX_C];
    snprintf(resolved, sizeof(resolved), "%s", path);

    for (size_t hops = 0; hops < 16; hops++) {
        const char *find = last_path_component(resolved);
        hard_link_t *step = insert_dir(resolved);

        while (step != NULL) {
            if (strcmp(step->name, find) == 0)
                break;
            step = step->next;
        }

        if (!step)
            return NULL;

        if (!follow_last_symlink || !step->meta || !step->meta->flags.is_symlink)
            return step;

        if (!step->meta->head)
            return NULL;

        size_t n = step->meta->head->memsize;
        if (n >= sizeof(resolved))
            n = sizeof(resolved) - 1;

        memcpy(resolved, step->meta->head->data, n);
        resolved[n] = '\0';
    }

    return NULL;
}

static int8_t __open_fd(const char *filepath) {
    hard_link_t *step = __find_path(filepath, 1);
    if (!step || !step->meta || step->meta->flags.is_dir)
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
    if (selected_fd >= MAX_FD || !fd[selected_fd] || !fd[selected_fd]->meta) {
        return -1;
    }

    fd[selected_fd]->meta->offset = 0;
    fd[selected_fd] = NULL;

    return 0;
}

static int8_t __seek_fd(const uint8_t selected_fd, const uint64_t offset) {
    if (selected_fd >= MAX_FD || !fd[selected_fd] || !fd[selected_fd]->meta)
        return -1;

    inode_t *selected_inode = fd[selected_fd]->meta;

    // Handle if offset is bigger, than file size
    if (selected_inode->file_size < offset) {
        size_t create_block = (offset - selected_inode->file_size) / MAX_B;
        selected_inode->file_size = create_block * MAX_B;
        block_t *step = selected_inode->head;

        if (!step)
            return -2;

        // Create new data block
        for (size_t i = create_block; i > 0; i--) {
            step->next = bl_create(step, NULL);
            step = step->next;
        }
    }

    selected_inode->offset = offset;

    return 0;    
}

static int8_t __write_fd(const uint8_t selected_fd, uint64_t size) {
    if (selected_fd >= MAX_FD || !fd[selected_fd] || !fd[selected_fd]->meta)
        return -1;

    inode_t *inode = fd[selected_fd]->meta;
    block_t *blk = inode->head;
    size_t left = (size > SIZE_MAX) ? SIZE_MAX : (size_t)size;
    size_t off = inode->offset;
    size_t skip_blocks = off / MAX_B;
    size_t in_block = off % MAX_B;

    while (skip_blocks--) {
        if (blk->next == NULL) {
            blk->next = bl_create(blk, NULL);
        }

        blk = blk->next;
    }

    while (left > 0) {
        size_t chunk = MAX_B - in_block; 

        if (chunk > left)
            chunk = left;

        memset(blk->data + in_block, 0, chunk);

        if (blk->memsize < in_block + chunk)
            blk->memsize = in_block + chunk;

        off += chunk;
        left -= chunk;
        in_block = 0;

        if (left > 0) {
            if (!blk->next) {
                blk->next = bl_create(blk, NULL);
                if (!blk->next)
                    return -3;
            }
        }

        blk = blk->next;
    }

    inode->offset = off;
    if (inode->file_size < off)
        inode->file_size = off;

    return 0;
}

static int8_t __read_fd(const uint8_t selected_fd, uint64_t size) {
    if (selected_fd >= MAX_FD || !fd[selected_fd] || !fd[selected_fd]->meta)
        return -1;

    inode_t *inode = fd[selected_fd]->meta;
    block_t *blk = inode->head;

    if (!blk)
        return -2;

    if (inode->offset >= inode->file_size)
        return 0;

    size_t left = (size > SIZE_MAX) ? SIZE_MAX : (size_t)size;
    size_t available = inode->file_size - inode->offset;

    if (left > available)
        left = available;

    size_t off = inode->offset;
    size_t skip_blocks = off / MAX_B;
    size_t in_block = off % MAX_B;

    while (skip_blocks--) {
        if (!blk->next)
            return -2;
        blk = blk->next;
    }

    while (left > 0) {
        size_t chunk = MAX_B - in_block;
        if (chunk > left)
            chunk = left;

        for (size_t i = 0; i < chunk; i++) {
            printf("%02X ", blk->data[in_block + i]);
        }

        off += chunk;
        left -= chunk;
        in_block = 0;

        if (left > 0) {
            if (!blk->next)
                break;
            blk = blk->next;
        }
    }

    inode->offset = off;
    putchar('\n');
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

static hard_link_t *insert_dir(char *path) {
    if (!path || !root || !user_pos)
        return NULL;

    if (strcmp(path, "/") == 0)
        return root->head;

    char copy[MAX_C];
    snprintf(copy, sizeof(copy), "%s", path);
    char **dirs = __split_string(copy, "/");

    dir_t *curr = path[0] == '/' ? root : user_pos;
    if (!dirs || !dirs[0])
        return curr->head;

    for (size_t i = 0; dirs[i + 1] != NULL; i++) {
        hard_link_t *step = curr->head;
        while (step != NULL) {
            if (step->meta->flags.is_dir && strcmp(step->name, dirs[i]) == 0)
                break;
            step = step->next;
        }

        if (!step || !step->meta || !step->meta->dhead)
            return NULL;

        curr = step->meta->dhead;
    }

    return curr->head;
}

// TODO: make equal name fallback
static int8_t __create_file(const char *name) {
    if (!name) 
        return -1;

    const char *fname = last_path_component(name);
    hard_link_t *dir_head = insert_dir((char *)name);
    if (!dir_head)
        return -1;

    id++;
    inode_t *newi = in_create(id, 0);
    hard_link_t *newh = hl_create(fname, newi);

    hard_link_t *tail = dir_head;
    while (tail->next != NULL)
        tail = tail->next;
    hl_add(tail, newh);

    return 0;
}

// TODO: add show directory by path
static int8_t __show_directory(void) {
    if (!root)
        return -1;

    hard_link_t *step = user_pos->head;

    if (!step)
        return -1;

    while (step != NULL) {
        printf("link %c [%lu]\t<\t%lu,\t%lu,\t%lu>:\t%s\n", 
            step->meta->flags.is_dir ? 'd' : 'f',
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

    hard_link_t *step = __find_path(path, 1);
    if (!step)
        return -2;

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

static int8_t __link(const char *link_filename, const char *new_filename) {
    if (!link_filename || !new_filename || !root)
        return -1;

    hard_link_t *link = __find_path(link_filename, 0);

    if (!link)
        return -2;
    if (link->meta->flags.is_dir)
        return -5;

    const char *find = last_path_component(new_filename);
    hard_link_t *dir_head = insert_dir((char *)new_filename);
    hard_link_t *step = dir_head;
    while (step != NULL) {
        if (strcmp(step->name, find) == 0)
            return -4;
        step = step->next;
    }

    hard_link_t *new_link = hl_create(find, link->meta);
    if (!new_link)
        return -3;

    new_link->meta->h_links_number++;
    if (!dir_head)
        return -3;

    hard_link_t *tail = dir_head;
    while (tail->next != NULL)
        tail = tail->next;
    hl_add(tail, new_link);

    return 0;
}

static int8_t __unlink(const char *filename) {
    if (!filename || !root)
        return -1;

    hard_link_t *link = __find_path(filename, 0);

    if (!link)
        return -2;
    if (link->meta->flags.is_dir)
        return -4;

    if (link == root->head)
        return -3;

    inode_t *inode = link->meta;
    inode->h_links_number--;

    if (inode->h_links_number == 0) 
       in_del(inode);

    if (link->prev)
        link->prev->next = link->next;

    if (link->next)
        link->next->prev = link->prev;

    if (root->tail == link)
        root->tail = link->prev;

    free(link->name);
    free(link);

    return 0;
}

static int8_t __truncate_file(const char *name, uint64_t size) {
    if (!name || !root)
        return -1;

    hard_link_t *link = __find_path(name, 1);

    if (!link)
        return -2;

    inode_t *inode = link->meta;
    size_t new_size = (size > SIZE_MAX) ? SIZE_MAX : (size_t)size;
    size_t old_size = inode->file_size;

    if (new_size > old_size) {
        block_t *blk = inode->head;
        if (!blk)
            return -3;

        size_t off = old_size;
        size_t skip_blocks = off / MAX_B;
        size_t in_block = off % MAX_B;
        size_t left = new_size - old_size;

        while (skip_blocks--) {
            if (!blk->next) {
                blk->next = bl_create(blk, NULL);
                if (!blk->next)
                    return -3;
            }
            blk = blk->next;
        }

        while (left > 0) {
            size_t chunk = MAX_B - in_block;
            if (chunk > left)
                chunk = left;

            memset(blk->data + in_block, 0, chunk);
            if (blk->memsize < in_block + chunk)
                blk->memsize = in_block + chunk;

            left -= chunk;
            in_block = 0;

            if (left > 0) {
                if (!blk->next) {
                    blk->next = bl_create(blk, NULL);
                    if (!blk->next)
                        return -3;
                }
                blk = blk->next;
            }
        }
    } else if (new_size < old_size) {
        size_t idx = 0;
        block_t *blk = inode->head;
        while (blk != NULL) {
            size_t block_start = idx * MAX_B;
            size_t block_end = block_start + MAX_B;

            if (block_start >= new_size) {
                memset(blk->data, 0, MAX_B);
                blk->memsize = 0;
            } else if (block_end > new_size) {
                size_t keep = new_size - block_start;
                memset(blk->data + keep, 0, MAX_B - keep);
                blk->memsize = keep;
            } else if (blk->memsize > MAX_B) {
                blk->memsize = MAX_B;
            }

            blk = blk->next;
            idx++;
        }
    }

    inode->file_size = new_size;
    if (inode->offset > new_size)
        inode->offset = new_size;

    return 0;
}

int8_t __create_dir(const char *path) {
    if (!path)
        return -1;

    const char *name = last_path_component(path);
    hard_link_t *dir_head = insert_dir((char *)path);
    if (!dir_head)
        return -1;

    id++;
    inode_t *newi = in_create(id, 0);
    newi->flags.is_dir = 1;
    newi->dhead = dir_create(id);
    newi->dhead->dprev = user_pos; 
    hard_link_t *newh = hl_create(name, newi);

    hard_link_t *tail = dir_head;
    while (tail->next != NULL)
        tail = tail->next;
    hl_add(tail, newh);

    return 0;
}

static int8_t __create_symlink(const char *target, const char *path) {
    if (!target || !path || strlen(target) > MAX_B)
        return -1;

    if (__find_path(path, 0))
        return -2;

    const char *name = last_path_component(path);
    hard_link_t *dir_head = insert_dir((char *)path);
    if (!dir_head)
        return -3;

    id++;
    inode_t *newi = in_create(id, strlen(target));
    if (!newi || !newi->head)
        return -4;

    newi->flags.is_symlink = 1;
    newi->head->memsize = strlen(target);
    memcpy(newi->head->data, target, newi->head->memsize);

    hard_link_t *newh = hl_create(name, newi);
    if (!newh)
        return -4;

    hard_link_t *tail = dir_head;
    while (tail->next != NULL)
        tail = tail->next;
    hl_add(tail, newh);

    return 0;
}

int8_t __remove_dir(const char *path) {
    if (!path)
        return -1;

    const char *find = last_path_component(path);
    hard_link_t *step = insert_dir((char *)path);

    while (step != NULL) {
        if (step->meta->flags.is_dir && strcmp(find, step->name) == 0) {
            if (step->meta->dhead && step->meta->dhead->head &&
                step->meta->dhead->head->next != NULL) {
                return -1; // directory is not empty
            }

            if (step->prev)
                step->prev->next = step->next;
            if (step->next)
                step->next->prev = step->prev;

            free(step->name);
            free(step);
            return 0;
        }

        step = step->next;
    }

    return -1;
}

int8_t __open_dir(const char *path) {
    if (!path)
        return -1;

    if (strcmp(path, "..") == 0) {
        if (user_pos->dprev)
            user_pos = user_pos->dprev;

        return 0;
    }

    const char *find = last_path_component(path);
    hard_link_t *step = insert_dir((char *)path);

    while (step != NULL) {
        if (step->meta->flags.is_dir && strcmp(find, step->name) == 0) {
            user_pos = step->meta->dhead;
            return 0;
        }

        step = step->next;
    }

    return -1;
}

int8_t fs_parse_command(char *comm_line) {
    if (!root) {
        root = dir_create(id);
        user_pos = root;
    }

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

    if (strcmp(command, "symlink") == 0) {
        if (!arguments[1] || !arguments[2]) {
            printf("Usage: symlink str pathname\n");
            return -1;
        }

        int8_t ret = __create_symlink(arguments[1], arguments[2]);
        if (ret == -1) {
            printf("Wrong symlink arguments\n");
            return -1;
        } else if (ret == -2) {
            printf("Path already exists\n");
            return -1;
        } else if (ret != 0) {
            printf("Symlink create error\n");
            return -1;
        }
        return 0;
    }

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
                case 'd':
                        if (__open_dir(arguments[1]) != 0) {
                            printf("error\n");
                            return -1;
                        }

                        return 0;
            }
            return -1;
        case 'l':
            if (command[1] == 'i') {
                if (!arguments[1] || !arguments[2]) {
                    printf("Enter source and new link names\n");
                    return -1;
                }

                int8_t ret = __link(arguments[1], arguments[2]);
                if (ret == -2) {
                    printf("Source file not found\n");
                    return -1;
                } else if (ret == -3) {
                    printf("Link create error\n");
                    return -1;
                } else if (ret == -4) {
                    printf("Name already exists\n");
                    return -1;
                } else if (ret == -5) {
                    printf("Can't link directory\n");
                    return -1;
                }

                printf("Link created: %s -> %s\n", arguments[2], arguments[1]);
                return 0;
            }

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
        case 'w': {
            char *endptr = NULL;
            unsigned int cfd, csize;
            if (!arguments[1] || !arguments[2]) {
                printf("Enter fd and size\n");
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

            csize = strtoul(
                arguments[2],
                &endptr,
                0
            );

            if (endptr == arguments[2] || *endptr != '\0') {
                printf("Enter size\n");
                return -1;
            }

            int8_t ret = __write_fd(cfd, csize);
            if (ret == -1) {
                printf("Wrong FD\n");
                return -1;
            } else if (ret == -3) {
                printf("Block error\n");
                return -1;
            }

            printf("Write %u bytes\n", csize);
            return 0;
        }
        case 'r': {
            if (arguments[0][1] == 'm')
                return __remove_dir(arguments[1]);

            char *endptr = NULL;
            unsigned int cfd, csize;
            if (!arguments[1] || !arguments[2]) {
                printf("Enter fd and size\n");
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

            csize = strtoul(
                arguments[2],
                &endptr,
                0
            );

            if (endptr == arguments[2] || *endptr != '\0') {
                printf("Enter size\n");
                return -1;
            }

            int8_t ret = __read_fd(cfd, csize);
            if (ret == -1) {
                printf("Wrong FD\n");
                return -1;
            } else if (ret == -2) {
                printf("Block error\n");
                return -1;
            }

            return 0;
        }
        case 'u':
            if (!arguments[1]) {
                printf("Enter filename\n");
                return -1;
            }

            switch (__unlink(arguments[1])) {
                case -2:
                    printf("File not found\n");
                    return -1;
                case -3:
                    printf("Can't unlink root entry\n");
                    return -1;
                case -4:
                    printf("Can't unlink directory (use rmdir)\n");
                    return -1;
            }

            printf("Unlinked %s\n", arguments[1]);
            return 0;
        case 't': {
            if (command[1] != 'r' || !arguments[1] || !arguments[2]) {
                printf("Usage: truncate <name> <size>\n");
                return -1;
            }

            char *endptr = NULL;
            unsigned long new_size = strtoul(arguments[2], &endptr, 0);
            if (endptr == arguments[2] || *endptr != '\0') {
                printf("Enter size\n");
                return -1;
            }

            int8_t ret = __truncate_file(arguments[1], new_size);
            if (ret == -2) {
                printf("File not found\n");
                return -1;
            } else if (ret == -3) {
                printf("Block error\n");
                return -1;
            }

            printf("Truncate %s to %lu bytes\n", arguments[1], new_size);
            return 0;
        }
        case 'm':
            if (__create_dir(arguments[1]) != 0) {
                return -1;
            }

            return 0;
    }

    return INT8_MAX - 1;
}
