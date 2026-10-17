#include "mm.h"
#include "kernel.h"
#include "string.h"
#include "printf.h"

/* -------------------------------------------------------------------------
 * Physical Page Frame Allocator (64 MB Managed RAM, 4 KB Pages)
 * ------------------------------------------------------------------------- */
static uintptr_t page_base_addr = 0;
static uint64_t page_bitmap[TOTAL_PAGES / 64];
static uint64_t allocated_pages_count = 0;

static inline void set_bit(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit / 64] |= (1ULL << (bit % 64));
}

static inline void clear_bit(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit / 64] &= ~(1ULL << (bit % 64));
}

static inline int test_bit(const uint64_t *bitmap, uint64_t bit) {
    return (bitmap[bit / 64] & (1ULL << (bit % 64))) != 0;
}

void *alloc_pages(size_t num_pages) {
    if (num_pages == 0 || num_pages > TOTAL_PAGES) {
        return NULL;
    }

    size_t consecutive = 0;
    size_t start_index = 0;

    for (size_t i = 0; i < TOTAL_PAGES; i++) {
        if (!test_bit(page_bitmap, i)) {
            if (consecutive == 0) {
                start_index = i;
            }
            consecutive++;
            if (consecutive == num_pages) {
                /* Found contiguous free block */
                for (size_t j = 0; j < num_pages; j++) {
                    set_bit(page_bitmap, start_index + j);
                }
                allocated_pages_count += num_pages;
                void *ptr = (void *)(page_base_addr + (start_index * PAGE_SIZE));
                memset(ptr, 0, num_pages * PAGE_SIZE);
                return ptr;
            }
        } else {
            consecutive = 0;
        }
    }

    kprintf("[MM:ERROR] Out of physical pages! Requested %u pages.\n", (unsigned int)num_pages);
    return NULL;
}

void *alloc_page(void) {
    return alloc_pages(1);
}

void free_pages(void *ptr, size_t num_pages) {
    if (!ptr || num_pages == 0) {
        return;
    }

    uintptr_t addr = (uintptr_t)ptr;
    if (addr < page_base_addr || addr >= page_base_addr + MANAGED_MEM_SIZE) {
        kprintf("[MM:ERROR] Invalid free_pages address: %p\n", ptr);
        return;
    }

    size_t start_index = (addr - page_base_addr) / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++) {
        size_t idx = start_index + i;
        if (idx < TOTAL_PAGES && test_bit(page_bitmap, idx)) {
            clear_bit(page_bitmap, idx);
            if (allocated_pages_count > 0) {
                allocated_pages_count--;
            }
        }
    }
}

void free_page(void *ptr) {
    free_pages(ptr, 1);
}

uint64_t mm_get_free_pages(void) {
    return TOTAL_PAGES - allocated_pages_count;
}

uint64_t mm_get_total_pages(void) {
    return TOTAL_PAGES;
}

/* -------------------------------------------------------------------------
 * Fine-Grained Dynamic Kernel Heap Allocator (kmalloc / kfree)
 * ------------------------------------------------------------------------- */

#define BLOCK_MAGIC 0x4B4D414CU /* "KMAL" */
#define ALIGN16(x)  (((x) + 15) & ~15)

struct block_header {
    uint32_t magic;
    uint32_t is_free;
    size_t size; /* Size of usable payload in bytes */
    struct block_header *next;
    struct block_header *prev;
};

#define HEADER_SIZE ALIGN16(sizeof(struct block_header))

static struct block_header *heap_head = NULL;
static size_t heap_total_allocated_bytes = 0;
static size_t heap_total_used_bytes = 0;

static int expand_heap(size_t min_bytes) {
    size_t required = min_bytes + HEADER_SIZE;
    size_t pages_needed = (required + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed < 4) {
        pages_needed = 4; /* Allocate at least 16 KB per expansion */
    }

    void *new_mem = alloc_pages(pages_needed);
    if (!new_mem) {
        return 0;
    }

    size_t total_expansion = pages_needed * PAGE_SIZE;
    heap_total_allocated_bytes += total_expansion;

    struct block_header *new_block = (struct block_header *)new_mem;
    new_block->magic = BLOCK_MAGIC;
    new_block->is_free = 1;
    new_block->size = total_expansion - HEADER_SIZE;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (!heap_head) {
        heap_head = new_block;
    } else {
        struct block_header *curr = heap_head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_block;
        new_block->prev = curr;

        /* Attempt coalesce with previous block if it is contiguous and free */
        uintptr_t curr_end = (uintptr_t)curr + HEADER_SIZE + curr->size;
        if (curr->is_free && curr_end == (uintptr_t)new_block) {
            curr->size += HEADER_SIZE + new_block->size;
            curr->next = new_block->next;
            if (new_block->next) {
                new_block->next->prev = curr;
            }
        }
    }

    return 1;
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t aligned_size = ALIGN16(size);

    struct block_header *curr = heap_head;
    while (curr) {
        if (curr->magic != BLOCK_MAGIC) {
            kprintf("[KMALLOC:PANIC] Heap corruption detected at block %p!\n", (void *)curr);
            return NULL;
        }

        if (curr->is_free && curr->size >= aligned_size) {
            /* Can we split this block? */
            if (curr->size >= aligned_size + HEADER_SIZE + 32) {
                struct block_header *split_block = 
                    (struct block_header *)((uintptr_t)curr + HEADER_SIZE + aligned_size);
                split_block->magic = BLOCK_MAGIC;
                split_block->is_free = 1;
                split_block->size = curr->size - aligned_size - HEADER_SIZE;
                split_block->next = curr->next;
                split_block->prev = curr;

                if (curr->next) {
                    curr->next->prev = split_block;
                }
                curr->next = split_block;
                curr->size = aligned_size;
            }

            curr->is_free = 0;
            heap_total_used_bytes += curr->size;
            return (void *)((uintptr_t)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }

    /* No suitable free block, expand heap */
    if (!expand_heap(aligned_size)) {
        kprintf("[KMALLOC:ERROR] Heap expansion failed for %u bytes!\n", (unsigned int)size);
        return NULL;
    }

    /* Retry allocation with expanded heap */
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }

    struct block_header *header = (struct block_header *)((uintptr_t)ptr - HEADER_SIZE);
    if (header->magic != BLOCK_MAGIC) {
        kprintf("[KFREE:ERROR] Invalid pointer or heap corruption at %p (magic: 0x%08x)\n",
                ptr, header->magic);
        return;
    }

    if (header->is_free) {
        kprintf("[KFREE:WARNING] Double free detected at %p!\n", ptr);
        return;
    }

    header->is_free = 1;
    if (heap_total_used_bytes >= header->size) {
        heap_total_used_bytes -= header->size;
    }

    /* Coalesce with next block if free */
    if (header->next && header->next->is_free) {
        uintptr_t next_addr = (uintptr_t)header + HEADER_SIZE + header->size;
        if (next_addr == (uintptr_t)header->next) {
            header->size += HEADER_SIZE + header->next->size;
            header->next = header->next->next;
            if (header->next) {
                header->next->prev = header;
            }
        }
    }

    /* Coalesce with prev block if free */
    if (header->prev && header->prev->is_free) {
        uintptr_t prev_end = (uintptr_t)header->prev + HEADER_SIZE + header->prev->size;
        if (prev_end == (uintptr_t)header) {
            header->prev->size += HEADER_SIZE + header->size;
            header->prev->next = header->next;
            if (header->next) {
                header->next->prev = header->prev;
            }
        }
    }
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct block_header *header = (struct block_header *)((uintptr_t)ptr - HEADER_SIZE);
    if (header->magic != BLOCK_MAGIC) {
        return NULL;
    }

    if (header->size >= new_size) {
        return ptr;
    }

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, header->size);
    kfree(ptr);
    return new_ptr;
}

void mm_dump_stats(void) {
    kprintf("--- Memory Allocator Statistics ---\n");
    kprintf("  Physical Memory Base: 0x%016lx\n", (uint64_t)page_base_addr);
    kprintf("  Total Managed RAM:    %u MB (%lu Pages of 4KB)\n",
            (unsigned int)(MANAGED_MEM_SIZE / (1024 * 1024)), mm_get_total_pages());
    kprintf("  Allocated Pages:      %lu (%lu KB)\n",
            allocated_pages_count, (allocated_pages_count * PAGE_SIZE) / 1024);
    kprintf("  Free Physical Pages:  %lu (%lu KB)\n",
            mm_get_free_pages(), (mm_get_free_pages() * PAGE_SIZE) / 1024);
    kprintf("  Heap Allocated Memory:%lu KB\n", heap_total_allocated_bytes / 1024);
    kprintf("  Heap Active Usage:    %lu bytes\n", heap_total_used_bytes);
}

uint64_t mm_get_heap_total_bytes(void) {
    return (uint64_t)heap_total_allocated_bytes;
}

uint64_t mm_get_heap_used_bytes(void) {
    return (uint64_t)heap_total_used_bytes;
}

void mm_init(void) {
    /* Align page allocation base to 4KB boundary above __heap_start */
    uintptr_t raw_heap = (uintptr_t)__heap_start;
    page_base_addr = (raw_heap + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Zero out page frame allocation bitmap */
    memset(page_bitmap, 0, sizeof(page_bitmap));
    allocated_pages_count = 0;

    /* Initialize heap with an initial pool of 16 pages (64 KB) */
    heap_head = NULL;
    heap_total_allocated_bytes = 0;
    heap_total_used_bytes = 0;
    expand_heap(16 * PAGE_SIZE);

    kprintf("[+] Physical Page Allocator initialized (%u MB at 0x%016lx)\n",
            (unsigned int)(MANAGED_MEM_SIZE / (1024 * 1024)), (uint64_t)page_base_addr);
    kprintf("[+] Dynamic Heap Allocator (kmalloc/kfree) initialized (Initial Heap: %lu KB)\n",
            heap_total_allocated_bytes / 1024);
}
