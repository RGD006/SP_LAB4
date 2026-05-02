#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include <stdint.h>

#define MAX_C (1024) // MAX command size

int8_t fs_parse_command(char *comm_line);

#endif