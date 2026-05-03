#include "directory.h"

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