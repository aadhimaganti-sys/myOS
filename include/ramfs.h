#ifndef _RAMFS_H
#define _RAMFS_H

#include "types.h"
#include "stddef.h"

#define RAMFS_MAX_FILES 32
#define RAMFS_MAX_NAME  32
#define RAMFS_MAX_SIZE  4096

struct ramfs_file {
    char name[RAMFS_MAX_NAME];
    size_t size;
    int is_used;
    char data[RAMFS_MAX_SIZE];
};

void ramfs_init(void);
int  ramfs_create(const char *name);
int  ramfs_write(const char *name, const char *data, size_t len);
int  ramfs_read(const char *name, char *buf, size_t buf_size);
int  ramfs_delete(const char *name);
void ramfs_list(void);
struct ramfs_file *ramfs_find(const char *name);

#endif /* _RAMFS_H */
