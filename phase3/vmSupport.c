#include "headers/vmSupport.h"
#include "headers/sysSupport.h"


// Definizioni
#define FRAMEPOOLSTART (RAMSTART + (OSFRAMES * PAGESIZE))

extern void klog_print(char*);
extern void klog_print_dec(int);
extern void klog_print_hex(unsigned int);

// Prototipi delle funzioni statiche
static void updateTLB(pteEntry_t* p);
static int isSwapFrameFree(int frame);
static int getSwapFrame();
static int getFifoFrame();

void pager() {
    klog_print("vmSupport: --- Inizio Pager (Page Fault) ---\n");
    support_t* sup = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t* current_state = &sup->sup_exceptState[PGFAULTEXCEPT];
    unsigned int p = ENTRYHI_GET_VPN(current_state->entry_hi);
    unsigned int asid = sup->sup_asid;
    pteEntry_t* pte_p = &sup->sup_privatePgTbl[p];

    int pid = SYSCALL(GETPROCESSID, 0, 0, 0); // Ottieni il PID per la gestione del mutex

    klog_print("vmSupport: Page Fault per ASID: ");
    klog_print_dec(asid);
    klog_print(", VPN: ");
    klog_print_hex(p);
    klog_print("\n");

    unsigned int cause = current_state->cause;
    if ((cause & CAUSE_EXCCODE_MASK) == EXC_MOD) {
        klog_print("vmSupport: ERRORE! Rilevata TLB-Modification exception. Terminazione...\n");
        programTrapHandler(current_state);
        return;
    };

    klog_print("vmSupport: Acquisizione mutex Swap Pool...\n");
    getMutex(&swap_pool_sem, pid);

    int victim = getSwapFrame();
    klog_print("vmSupport: Frame scelto per il rimpiazzo (vittima): ");
    klog_print_dec(victim);
    klog_print("\n");

    if (!isSwapFrameFree(victim)) {
        int x_asid = swap_pool_table[victim].sw_asid;
        unsigned int k_vpn = swap_pool_table[victim].sw_pageNo;
        pteEntry_t* k_pte = swap_pool_table[victim].sw_pte;

        klog_print("vmSupport: Frame occupato da ASID: ");
        klog_print_dec(x_asid);
        klog_print(", VPN: ");
        klog_print_hex(k_vpn);
        klog_print(". Avvio scrittura su flash (page-out)...\n");

        CRITICAL_START();
        k_pte->pte_entryLO &= ~VALIDON; // Invalida la vecchia pagina
        updateTLB(k_pte);
        CRITICAL_END();

        read_or_write_flash(victim, k_vpn, x_asid, FLASHWRITE);
        klog_print("vmSupport: Page-out completato.\n");
    } else {
        klog_print("vmSupport: Trovato frame libero, non e' necessario un page-out.\n");
    }

    klog_print("vmSupport: Avvio lettura da flash della nuova pagina (page-in)...\n");
    read_or_write_flash(victim, p, asid, FLASHREAD);
    klog_print("vmSupport: Page-in completato.\n");

    klog_print("vmSupport: Aggiornamento delle strutture dati (Swap Pool Table e Page Table)...\n");
    update_swap_pool_entry(victim, p, asid, pte_p);

    CRITICAL_START();
    pte_p->pte_entryLO |= VALIDON;
    pte_p->pte_entryLO &= ~ENTRYLO_PFN_MASK; // Pulisce il vecchio PFN
    pte_p->pte_entryLO |= (FRAMEPOOLSTART + (victim * PAGESIZE)); // Imposta il nuovo PFN
    updateTLB(pte_p);
    CRITICAL_END();
    klog_print("vmSupport: Strutture aggiornate.\n");

    
    klog_print("vmSupport: Rilascio mutex Swap Pool.\n");
    releaseMutex(&swap_pool_sem, pid);

    klog_print("vmSupport: --- Fine Pager. Ritorno al processo. ---\n");
    LDST(current_state);
}

void read_or_write_flash(int frame_i, int vpn, int asid, int op) {
    unsigned int frame_phys = FRAMEPOOLSTART + (frame_i * PAGESIZE);
    dtpreg_t* flash = (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, asid - 1);
    flash->data0 = frame_phys;
    unsigned int cmd = ((unsigned int)vpn << 8) | op;

    klog_print("  flash_io: Esecuzione DOIO su flash. ASID: ");
    klog_print_dec(asid);
    klog_print(", VPN/Blocco: ");
    klog_print_hex(vpn);
    klog_print(", Operazione: ");
    klog_print_dec(op);
    klog_print("\n");

    int status = SYSCALL(DOIO, (int)&(flash->command), cmd, 0);

    if (status != 1) { // 1 == READY
        klog_print("  flash_io: ERRORE CRITICO! I/O su flash fallito! Status: ");
        klog_print_dec(status);
        klog_print(". Avvio Program Trap Handler...\n");
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
    setENTRYLO(p->pte_entryLO);
    TLBWR();
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