#include "interface.h"
#include "vmm.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

// #define __ENABLE_DEBUG__

// Interface implementation
// Implement APIs here...
static mmu_t mmu;


void handler(int signal, siginfo_t* siginfo, void* uap) {
#ifdef __ENABLE_DEBUG__
    printf("Attempt to access memory at address %p\n", siginfo->si_addr - mmu.kernel_mem_base);
#endif
    uint64_t pfn = PAGE_FLOOR(siginfo->si_addr - (unsigned long)mmu.kernel_mem_base) / mmu.page_size;
    if ((((ucontext_t *const)uap)->uc_mcontext.gregs[REG_ERR] & 2)) {
        load_page(&mmu, pfn, siginfo->si_addr, PROT_WRITE);
    }
    else {
        load_page(&mmu, pfn, siginfo->si_addr, PROT_READ);
    }
}


void mm_init(enum policy_type policy, void *vm, int vm_size, int num_frames, int page_size)
{
    // sigaction holds the singals
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    // there is no signal in the set
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = handler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
 // send signal to the function 

    sigaction(SIGSEGV, &act, NULL); //triggers the holder
    sigaction(SIGBUS, &act, NULL);
    sigaction(SIGTRAP, &act, NULL);
// protect the holder, set its access 
//it sets the signal to prot-none from other signal it only reaed and write
    mprotect((void *)PAGE_FLOOR(vm), vm_size, PROT_NONE);

    mmu_init(&mmu, policy, vm, vm_size, num_frames, page_size);
}
