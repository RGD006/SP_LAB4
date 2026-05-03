#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_B (512) // max block size in bytes

typedef struct _block_t {
    uint8_t data[MAX_B];
    struct _block_t *next;
    struct _block_t *prev;
} block_t;

typedef struct _inode_t {
    size_t id;
    size_t file_size;
    size_t h_links_number;
    time_t create_time;
    struct flags {
        uint8_t is_open: 1;
    } flags;
    block_t *head; // pointer to start of file 
} inode_t;

typedef struct _hard_link_t {
    char *name; 
    struct _hard_link_t *next;
    struct _hard_link_t *prev;
    inode_t *meta;
} hard_link_t;

block_t *bl_create(block_t *prev, block_t *next);
void bl_delete(block_t *block);
inode_t *in_create(const size_t _id, const size_t _file_size);
hard_link_t *hl_create(const char *_name, inode_t *_inode);
void hl_add(hard_link_t *prev, hard_link_t *next);
void hl_del(hard_link_t *hl);

#endif