#ifndef VMM_H
#define VMM_H

#include "interface.h"
#include <stdlib.h>

// #define __ENABLE_DEBUG__

// Declare your own data structures and functions here...
typedef struct {
    uint64_t phy_address;
    uint64_t pfn;
    uint64_t exec: 1;
    uint64_t write: 1;
    uint64_t read: 1;
    uint64_t dirty: 1;
    uint64_t accessed: 1;
    uint64_t valid: 1;
    uint64_t reference: 2;
    uint64_t modified: 1;
} pageframe_t;

typedef struct pglist {
    pageframe_t *frame;
    uint32_t reference;
    uint32_t modified;
    struct pglist *next;
    struct pglist *prev;
} pglist_t;

typedef struct {
    void *kernel_mem_base;
    pageframe_t *pageframes;
    uint64_t frame_number;
    uint64_t page_number;
    int page_size;
    int vm_size;

    enum {
        MMU_FIFO,
        MMU_THIRD,
    } mode;

    pglist_t fifo_head;
    uint64_t fifo_len;

    pageframe_t **pageframe_in_memory;
    uint64_t pageframe_pointer;

    pglist_t *tc_ptr;
    uint64_t tc_len;
} mmu_t;


extern int mmu_init(mmu_t *, enum policy_type, void *, int, int, int);
int is_page_loaded(mmu_t *, uint64_t);

uint64_t load_page(mmu_t *mmu, uint64_t pfn, void *phy_address, uint64_t flag);
#endif
