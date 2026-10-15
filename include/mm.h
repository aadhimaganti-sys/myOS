#ifndef _MM_H
#define _MM_H

#include "types.h"
#include "stddef.h"

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define TOTAL_PAGES     16384       /* 64 MB / 4 KB = 16384 pages */
#define MANAGED_MEM_SIZE (TOTAL_PAGES * PAGE_SIZE) /* 64 MB */

/* Physical Page Frame Allocator APIs */
void mm_init(void);
void *alloc_page(void);
void free_page(void *ptr);
void *alloc_pages(size_t num_pages);
void free_pages(void *ptr, size_t num_pages);
uint64_t mm_get_free_pages(void);
uint64_t mm_get_total_pages(void);

/* Fine-grained Kernel Heap Allocator APIs */
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void mm_dump_stats(void);

#endif /* _MM_H */
