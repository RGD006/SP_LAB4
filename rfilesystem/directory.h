#ifndef __DIRECTORY_H__
#define __DIRECTORY_H__

#include "block.h"

typedef struct _dir_t {
    hard_link_t *head;
    hard_link_t *tail;    
} dir_t;

dir_t *dir_create(const size_t id);
dir_t *dir_delete();
void dir_add_hl(dir_t *dir, hard_link_t *hl);
void dir_del_hl(dir_t *dir, hard_link_t *hl);

#endif