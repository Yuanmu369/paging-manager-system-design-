#include "vmm.h"

// #define __ENABLE_DEBUG__


// Memory Manager implementation
// Implement all other functions here..
void mmu_fifo_init(mmu_t *mmu, enum policy_type policy, void *vm, int vm_size, int num_frames, int page_size) {
#ifdef __ENABLE_DEBUG__
    puts("init fifo ...");
#endif

    mmu->mode = MMU_FIFO;
    // mmu->fifo_head.next = &(mmu->fifo_head);
    // mmu->fifo_head.prev = &(mmu->fifo_head);
    // mmu->fifo_len = 0;

    mmu->pageframe_pointer = 0;
    mmu->pageframe_in_memory = (pageframe_t **)malloc(sizeof(pageframe_t *) * mmu->frame_number);
    memset(mmu->pageframe_in_memory, 0, sizeof(pageframe_t *) * mmu->frame_number);

    /*
    for (int i = 0; i < mmu->frame_number; i++) {
        printf("%lld\n", mmu->pageframe_in_memory[i]);
    }
    exit(0);
    */
}

void mmu_third_init(mmu_t *mmu, enum policy_type policy, void *vm, int vm_size, int num_frames, int page_size) {
#ifdef __ENABLE_DEBUG__
    puts("init third chance ...");
#endif

    mmu->mode = MMU_THIRD;

    mmu->tc_len = 0;
    mmu->pageframe_pointer = 0;
    mmu->pageframe_in_memory = (pageframe_t **)malloc(sizeof(pageframe_t *) * mmu->frame_number);
    memset(mmu->pageframe_in_memory, 0, sizeof(pageframe_t *) * mmu->frame_number);

#ifdef __ENABLE_DEBUG__
    printf("frames in memory : ");
    for (int i = 0; i < mmu->frame_number; i++) {
        printf("%d-->%llx   ", i, mmu->pageframe_in_memory[i]);
    }
    putchar('\n');
#endif
    // mmu->tc_ptr = NULL;
}


int pageframe_log(mmu_t *mmu, uint64_t pfn) {
    printf("PFN %llu : ", pfn);
    printf("    phy address: %llx\n", mmu->pageframes[pfn].phy_address);
    printf("    pfn: %llu\n", mmu->pageframes[pfn].pfn);
    printf("    read: %llu\n", mmu->pageframes[pfn].read);
    printf("    write: %llu\n", mmu->pageframes[pfn].write);
    printf("    exec: %llu\n", mmu->pageframes[pfn].exec);
    printf("    valid: %llu\n", mmu->pageframes[pfn].valid);
}


#define PROCESS_MM_MODE_BRANCH(mode) {do {    \
    switch(mode) {                  \
        case MMU_FIFO:              \
            goto fifo;              \
            break;                  \
        case MMU_THIRD:             \
            goto third;             \
            break;                  \
        default:                    \
            break;                  \
    }                               \
} while(0);}


int is_page_loaded(mmu_t *mmu, uint64_t pfn) {
    PROCESS_MM_MODE_BRANCH(mmu->mode);

fifo:
    // pglist_t *pg = mmu->fifo_head.next;
    // while (pg != &(mmu->fifo_head)) {
    //     if (pg->frame->pfn == pfn) {
    //         return pfn;
    //     }
    //     pg = pg->next;
    // }

    for (uint64_t i = 0; i < mmu->pageframe_pointer; i++) {
        if (mmu->pageframe_in_memory[i] && mmu->pageframe_in_memory[i]->pfn == pfn)
            return i;
    }

    return -1;

third:
    for (uint64_t i = 0; i < mmu->frame_number; i++) {
#ifdef __ENABLE_DEBUG__
        printf("scanning %llu\n", i);
#endif
        if (mmu->pageframe_in_memory[i] && mmu->pageframe_in_memory[i]->pfn == pfn)
            return i;
    }

#ifdef __ENABLE_DEBUG__
    printf("Missed pfn %lld\n", pfn);
#endif

    return -1;
}


uint64_t load_page(mmu_t *mmu, uint64_t pfn, void *phy_address, uint64_t flag) {
    // if (pfn >= mmu->page_number)
        // return -1;
int index = is_page_loaded(mmu, pfn);
int dst = is_page_loaded(mmu, pfn);
#ifdef __ENABLE_DEBUG__
    for (int i = 0; i < mmu->frame_number; i++) {
        if (mmu->pageframe_in_memory[i])
            pageframe_log(mmu, mmu->pageframe_in_memory[i]->pfn);
    }
#endif

    int evicted = -1;
    int write_back = 0;
    int shift = 0;
    int type = 0;

    PROCESS_MM_MODE_BRANCH(mmu->mode);
fifo:
    
    if (index < 0) {
        
        // pglist_t *pg_item = (pglist_t *)malloc(sizeof(pglist_t));
        // pg_item->frame = &mmu->pageframes[pfn];
        // pg_item->frame->valid = 1;

        // pg_item->next = mmu->fifo_head.next;
        // pg_item->prev = &(mmu->fifo_head);
        // pg_item->next->prev = pg_item;
        // mmu->fifo_head.next = pg_item;
        // mmu->fifo_len++;

        if (mmu->pageframe_in_memory[mmu->pageframe_pointer]) { // evicte
            pageframe_t *pg = mmu->pageframe_in_memory[mmu->pageframe_pointer];
            evicted = pg->pfn;

            if (pg->dirty) {
                pg->dirty = 0;
                write_back = 1;
            }
            pg->valid = 0;

            pg->dirty = 0;
            pg->accessed = 0;
            pg->read = 0;
            pg->write = 0;

            mprotect((void *)(pg->phy_address), mmu->page_size, PROT_NONE);

            mmu->pageframe_in_memory[mmu->pageframe_pointer] = NULL;
        }

        // load page
        mmu->pageframe_in_memory[mmu->pageframe_pointer] = &mmu->pageframes[pfn];
        mmu->pageframes[pfn].valid = 1;

        if (flag & PROT_READ) {

            mmu->pageframes[pfn].accessed = 1;
            mmu->pageframes[pfn].read = 1;
        }
        
        if (flag & PROT_WRITE) {
            mmu->pageframes[pfn].write = 1;
            mmu->pageframes[pfn].dirty = 1;
        }


        mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, flag);
        index = mmu->pageframe_pointer;

        mmu->pageframe_pointer = (mmu->pageframe_pointer + 1) % mmu->frame_number;
        type = (flag & PROT_WRITE) > 0;
    }
    else {
        if (flag & PROT_WRITE) { // catch write event
            type = mmu->pageframes[pfn].write && mmu->pageframes[pfn].read && mmu->pageframes[pfn].valid ? 4 : 2;
            mmu->pageframes[pfn].dirty = 1;
            // mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_WRITE | PROT_READ);
        }
        else if (flag & PROT_READ) { // catch read event
            type = mmu->pageframes[pfn].valid && (mmu->pageframes[pfn].write || mmu->pageframes[pfn].read) ? 3 : 0;
            // mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_WRITE | PROT_READ);
        }


        // if (flag & PROT_WRITE) {
        //     mmu->pageframe[pfn].dirty = 1;
        // }

        // mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_WRITE | PROT_READ);
        switch (type) {
            case 3:
                mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_READ);
                mm_logger(PAGE_FLOOR(phy_address - (unsigned long)mmu->kernel_mem_base) / mmu->page_size, type, evicted, write_back, shift + (index << 12));
                break;
            case 4:
                mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_WRITE);
                mm_logger(PAGE_FLOOR(phy_address - (unsigned long)mmu->kernel_mem_base) / mmu->page_size, type, evicted, write_back, shift + (index << 12));
                break;
            default:
                mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_READ | PROT_WRITE);
                break;
        }
    }

    /*
    if (mmu->fifo_len > mmu->frame_number) {
        mmu->fifo_len--;
        pglist_t *to_del = mmu->fifo_head.prev;

        write_back = to_del->frame->dirty;

        to_del->prev->next = to_del->next;
        to_del->next->prev = to_del->prev;
        to_del->frame->valid = 0;

        evicted = to_del->frame->pfn;
        mprotect((void *)(to_del->frame->phy_address), mmu->page_size, PROT_NONE);

        free(to_del);
    }
    */

    shift = (uint64_t)(phy_address - (unsigned long)mmu->kernel_mem_base) & (mmu->page_size - 1);//页面偏移
    mm_logger(PAGE_FLOOR(phy_address - (unsigned long)mmu->kernel_mem_base) / mmu->page_size, type, evicted, write_back, shift + (index << 12));

    return pfn;

third:
    

    if (dst >= 0) { // page in memory
        while (mmu->pageframe_pointer != dst) {
#ifdef __ENABLE_DEBUG__
            printf("Looking for %lld, current %lld, pageframe numbers %lld ...\n", dst, mmu->pageframe_pointer, mmu->frame_number);
#endif
            pageframe_t *pg = mmu->pageframe_in_memory[mmu->pageframe_pointer];
            
            if (!pg) {
                mmu->pageframe_pointer = (mmu->pageframe_pointer + 1) % mmu->frame_number;
                continue;
            }

            if (pg->reference) {
                pg->reference--;

                if (pg->reference == 0) {
                    mprotect((void *)(pg->phy_address), mmu->page_size, PROT_NONE);
                }
            }
            else {
                // evacit??
                // write_back = pg->modified;
                // evicted = pg->pfn;
                // pg->valid = 0;
                // pg->reference = 0;
                // pg->modified = 0;

                // mprotect((void *)(pg->phy_address), mmu->page_size, PROT_NONE);
                // mmu->pageframe_in_memory[current_pointer] = NULL;
            }
            mmu->pageframe_pointer = (mmu->pageframe_pointer + 1) % mmu->frame_number;
        }

        // process dst page
        pageframe_t *pg = mmu->pageframe_in_memory[mmu->pageframe_pointer];
        if (flag & PROT_READ) {
            type = pg->valid && (pg->write || pg->read) ? 3 : 0;
            pg->reference = 1;

        }
        else if (flag & PROT_WRITE) {
            type = pg->write && pg->read && pg->valid ? 4 : 2;
            pg->reference = 2;
            pg->modified = 1;
        }

        shift = (uint64_t)(phy_address - (unsigned long)mmu->kernel_mem_base) & (mmu->page_size - 1);
        switch (type) {
            case 3:
                mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_READ);
                mm_logger(PAGE_FLOOR(phy_address - (unsigned long)mmu->kernel_mem_base) / mmu->page_size, type, evicted, write_back, shift + (index << 12));
                break;
            case 4:
                mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_WRITE);
                mm_logger(PAGE_FLOOR(phy_address - (unsigned long)mmu->kernel_mem_base) / mmu->page_size, type, evicted, write_back, shift + (index << 12));
                break;
            default:
                mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_READ | PROT_WRITE);
                break;
        }
    }
    else { // page in disk
        while (1) {
#ifdef __ENABLE_DEBUG__
            printf("check %lld from disk\n", mmu->pageframe_pointer);
#endif
            pageframe_t *pg = mmu->pageframe_in_memory[mmu->pageframe_pointer];
            if (!pg)
                break;

            if (pg->reference) {
                pg->reference--;

                if (pg->reference == 0) {
                    mprotect((void *)(pg->phy_address), mmu->page_size, PROT_NONE);
                }
            }
            else {
                // evacit
                if (evicted < 0) {
                    write_back = pg->modified;
                    evicted = pg->pfn;
                    pg->valid = 0;
                    pg->write = 0;
                    pg->read = 0;
                    pg->reference = 0;
                    pg->modified = 0;

                    mprotect((void *)(pg->phy_address), mmu->page_size, PROT_NONE);
                    mmu->pageframe_in_memory[mmu->pageframe_pointer] = NULL;

                    break;
                }
            }

            mmu->pageframe_pointer = (mmu->pageframe_pointer + 1) % mmu->frame_number;
        }

        // load the page
        mmu->pageframe_in_memory[mmu->pageframe_pointer] = &mmu->pageframes[pfn];
        mmu->pageframe_in_memory[mmu->pageframe_pointer]->valid = 1;

        if (flag & PROT_WRITE) {
            mmu->pageframe_in_memory[mmu->pageframe_pointer]->reference = 2;
            mmu->pageframe_in_memory[mmu->pageframe_pointer]->modified = 1;

            mmu->pageframe_in_memory[mmu->pageframe_pointer]->valid = 1;
            mmu->pageframe_in_memory[mmu->pageframe_pointer]->write = 1;

            mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_READ);
        }
        else if (flag & PROT_READ) {
            mmu->pageframe_in_memory[mmu->pageframe_pointer]->reference = 1;

            mmu->pageframe_in_memory[mmu->pageframe_pointer]->valid = 1;
            mmu->pageframe_in_memory[mmu->pageframe_pointer]->read = 1;

            mprotect((void *)PAGE_FLOOR(phy_address), mmu->page_size, PROT_WRITE);
        }


        shift = (uint64_t)(phy_address - (unsigned long)mmu->kernel_mem_base) & (mmu->page_size - 1);
        mm_logger(PAGE_FLOOR(phy_address - (unsigned long)mmu->kernel_mem_base) / mmu->page_size, (flag & PROT_WRITE) > 0, evicted, write_back, shift + (mmu->pageframe_pointer << 12));
    }

    // still needs a step
    mmu->pageframe_pointer = (mmu->pageframe_pointer + 1) % mmu->frame_number;

    return pfn;
}


int mmu_init(mmu_t *mmu, enum policy_type policy, void *vm, int vm_size, int num_frames, int page_size) {
    mmu->page_size = page_size;
    mmu->frame_number = num_frames;
    mmu->vm_size = vm_size;
    mmu->kernel_mem_base = vm;

    mmu->page_number = PAGE_CEILING(vm_size) / page_size;

    mmu->pageframes = (pageframe_t *)malloc(sizeof(pageframe_t) * mmu->page_number);
    memset(mmu->pageframes, 0, sizeof(pageframe_t) * mmu->page_number);
    for (uint64_t pfn = 0; pfn < mmu->page_number; pfn++) {
        mmu->pageframes[pfn].phy_address = pfn * page_size + PAGE_FLOOR(vm);
        mmu->pageframes[pfn].pfn = pfn;
        mmu->pageframes[pfn].exec = 0;
        mmu->pageframes[pfn].write = 0;
        mmu->pageframes[pfn].read = 0;
        mmu->pageframes[pfn].valid = 0;
    }

    switch (policy) {
        case MM_FIFO:
            mmu_fifo_init(mmu, policy, vm, vm_size, num_frames, page_size);
            break;
        case MM_THIRD:
            mmu_third_init(mmu, policy, vm, vm_size, num_frames, page_size);
            break;
        default:
            break;
    }
}












