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
    memset(block->data, 0, MAX_B);
    block->prev->next = block->next;
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
    newi->offset = 0;
    newi->head = bl_create(NULL, NULL);

    return newi;
}

void in_del(inode_t *inode) {
    block_t *blk = inode->head;

    while (!blk) {
        blk = blk->next;
    }

    while (!blk) {
        blk = blk->prev;
        bl_delete(blk->next);
    }

    bl_delete(blk);

    free(inode);
}

hard_link_t *hl_create(const char *_name, inode_t *_inode) {
    hard_link_t *newh = malloc(sizeof(hard_link_t));
   
    if (!newh)
        return NULL;

    newh->name = (char *)calloc(strlen(_name), sizeof(char));
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
