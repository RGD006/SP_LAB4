#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_B (10) // max block size in bytes

typedef struct _block_t {
    size_t memsize;
    uint8_t data[MAX_B];
    struct _block_t *next;
    struct _block_t *prev;
} block_t;

typedef struct _inode_t inode_t;

typedef struct _hard_link_t {
    char *name; 
    struct _hard_link_t *next;
    struct _hard_link_t *prev;
    inode_t *meta;
} hard_link_t;

typedef struct _dir_t {
    struct _dir_t *dprev;
    hard_link_t *head;
    hard_link_t *tail;    
} dir_t;

typedef struct _inode_t {
    size_t id;
    size_t file_size;
    size_t h_links_number;
    size_t offset;
    time_t create_time;
    struct flags {
        uint8_t is_open: 1;
        uint8_t is_dir: 1;
    } flags;
    dir_t *dhead;
    block_t *head; // pointer to start of file 
} inode_t;

block_t *bl_create(block_t *prev, block_t *next);
void bl_delete(block_t *block);
inode_t *in_create(const size_t _id, const size_t _file_size);
void in_del(inode_t *inode);
hard_link_t *hl_create(const char *_name, inode_t *_inode);
void hl_add(hard_link_t *prev, hard_link_t *next);
void hl_del(hard_link_t *hl);
dir_t *dir_create(const size_t id);
dir_t *dir_delete();
void dir_add_hl(dir_t *dir, hard_link_t *hl);
void dir_del_hl(dir_t *dir, hard_link_t *hl);
#endif