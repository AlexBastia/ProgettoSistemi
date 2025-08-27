#include "headers/vmSupport.h"
#include "headers/sysSupport.h"


extern void klog_print(char*);
extern void klog_print_dec(int);
extern void klog_print_hex(unsigned int);

// Prototipi delle funzioni statiche
static void updateTLB(pteEntry_t* p);
static int isSwapFrameFree(int frame);
static int getSwapFrame();
static int getFifoFrame();

void pager() {
    support_t* sup = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t* current_state = &sup->sup_exceptState[PGFAULTEXCEPT];
    unsigned int asid = sup->sup_asid;
    int pid = SYSCALL(GETPROCESSID, 0, 0, 0);
    unsigned int faulting_vpn = current_state->entry_hi >> VPNSHIFT;
    unsigned int page_tbl_index;
    if (faulting_vpn == (USERSTACKTOP >> VPNSHIFT) - 1) {
        page_tbl_index = MAXPAGES - 1;
    } else {
        page_tbl_index = faulting_vpn - (UPROCSTARTADDR >> VPNSHIFT);
    }
    if (page_tbl_index < 0 || page_tbl_index >= MAXPAGES) {
        programTrapHandler(current_state);
        return;
    }
    pteEntry_t* pte_p = &sup->sup_privatePgTbl[page_tbl_index];
    unsigned int cause = current_state->cause;
    if ((cause & CAUSE_EXCCODE_MASK) == EXC_MOD) {
        programTrapHandler(current_state);
        return;
    };   
     getMutex(&swap_pool_sem, pid);

    int victim = getSwapFrame();
    int page_out_needed = !isSwapFrameFree(victim);
    int x_asid = 0, k_vpn = 0;
    pteEntry_t* k_pte = NULL;

    if (page_out_needed) {
        x_asid = swap_pool_table[victim].sw_asid;
        k_vpn = swap_pool_table[victim].sw_pageNo;
        k_pte = swap_pool_table[victim].sw_pte;
        k_pte->pte_entryLO &= ~VALIDON;
        CRITICAL_START();
        updateTLB(k_pte);
        read_or_write_flash(victim, k_vpn, x_asid, FLASHWRITE);
        CRITICAL_END();

    }


    CRITICAL_START();
    
    read_or_write_flash(victim, page_tbl_index, asid, FLASHREAD);
    update_swap_pool_entry(victim, page_tbl_index, asid, pte_p);

    unsigned int pfn = (FRAMEPOOLSTART + (victim * PAGESIZE)) >> VPNSHIFT;

/* * CORREZIONE DEFINITIVA:
 * Costruiamo pte_entryLO da zero in un unico passaggio.
 * Questo è il modo corretto e robusto: combina il PFN (riportato a indirizzo base),
 * il bit di validità e il bit di "dirty".
 */
    pte_p->pte_entryLO = (pfn << VPNSHIFT) | VALIDON | DIRTYON;

    updateTLB(pte_p);
    CRITICAL_END();
    releaseMutex(&swap_pool_sem, pid);
    klog_print("vmSupport: --- Fine Pager. Ritorno al processo. ---\n");
    LDST(current_state);
}

void read_or_write_flash(int frame_i, int vpn, int asid, int op) {
    unsigned int frame_phys = FRAMEPOOLSTART + (frame_i * PAGESIZE);
    dtpreg_t* flash = (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, asid - 1);
    flash->data0 = frame_phys;
    unsigned int cmd = ((unsigned int)vpn << 8) | op;

    int status = SYSCALL(DOIO, (int)&(flash->command), cmd, 0);

    if (status != 1) { // 1 == READY
        support_t* support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
        state_t* exp_state = &(support->sup_exceptState[GENERALEXCEPT]);
        programTrapHandler(exp_state);
    } else {
        klog_print("  flash_io: Operazione I/O su flash completata con successo.\n");
    }
}

void update_swap_pool_entry(int frame_i, int vpn, int asid, pteEntry_t* pte) {
    swap_pool_table[frame_i].sw_pageNo = vpn;
    swap_pool_table[frame_i].sw_asid = asid;
    swap_pool_table[frame_i].sw_pte = pte;
}

static void updateTLB(pteEntry_t* p) {
  setENTRYHI(p->pte_entryHI);
    TLBP();
    if (!(getINDEX() & PRESENTFLAG)) { // Index.P == 0 significa entry presente
        setENTRYLO(p->pte_entryLO);
        TLBWI();
    }
}

static int isSwapFrameFree(int frame) {
    return swap_pool_table[frame].sw_asid == -1;
}

static int getFifoFrame() {
    static int fifo_hand = 0;
    int frame = fifo_hand;
    fifo_hand = (fifo_hand + 1) % POOLSIZE;
    return frame;
}

static int getSwapFrame() {
    for (int i = 0; i < POOLSIZE; i++) {
        if (isSwapFrameFree(i)) {
            return i;
        }
    }
    return getFifoFrame();
}