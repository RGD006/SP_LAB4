#include "block.h"

block_t *bl_create(block_t *prev, block_t *next) {
    block_t *newb = malloc(sizeof(block_t));

    if (!newb)
        return NULL;

    newb->prev = prev;
    newb->next = next;
    newb->memsize = 0;

    return newb;
}

void bl_delete(block_t *block) {
    if (!block)
        return;

    memset(block->data, 0, MAX_B);
    if (block->prev)
        block->prev->next = block->next;
    if (block->next)
        block->next->prev = block->prev;
    free(block);
}

inode_t *in_create(const size_t _id, const size_t _file_size) {
    inode_t *newi = malloc(sizeof(inode_t));

    if (!newi)
        return NULL;

    newi->id = _id;
    newi->file_size = _file_size;
    newi->h_links_number = 1;
    newi->create_time = time(NULL);
    newi->flags.is_open = 0;
    newi->flags.is_symlink = 0;
    newi->offset = 0;
    newi->head = bl_create(NULL, NULL);

    return newi;
}

void in_del(inode_t *inode) {
    block_t *blk = inode ? inode->head : NULL;
    while (blk) {
        block_t *next = blk->next;
        free(blk);
        blk = next;
    }
    free(inode);
}

hard_link_t *hl_create(const char *_name, inode_t *_inode) {
    hard_link_t *newh = malloc(sizeof(hard_link_t));
   
    if (!newh)
        return NULL;

    newh->name = (char *)calloc(strlen(_name) + 1, sizeof(char));
    strcpy(newh->name, _name);
    newh->meta = _inode;
    newh->next = NULL;
    newh->prev = NULL;

    return newh;
}

void hl_add(hard_link_t *prev, hard_link_t *next) {
    prev->next = next;
    next->prev = prev;
}

void hl_del(hard_link_t *hl) {
    hl->prev->next = hl->next;
    hl->next->prev = hl->prev;
}

dir_t *dir_create(const size_t _id) {
    dir_t *newd = malloc(sizeof(dir_t));

    if (!newd)
        return NULL;

    newd->head = hl_create(".", in_create(_id, MAX_B)); // create inode of this directory
    newd->tail = newd->head;

    return newd;
}

void dir_add_hl(dir_t *dir, hard_link_t *hl) {
    hl_add(dir->tail, hl);
    dir->tail = hl;
}

void dir_del_hl(dir_t *dir, hard_link_t *hl) {
    
}