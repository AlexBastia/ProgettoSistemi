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
    klog_print("\n\n---- MODIFICA DEL 23 AGOSTO COMPILATA CORRETTAMENTE ----\n\n"); // <-- AGGIUNGI QUESTA RIGA
    klog_print("vmSupport: --- Inizio Pager (Page Fault) ---\n");
    support_t* sup = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t* current_state = &sup->sup_exceptState[PGFAULTEXCEPT];
    unsigned int asid = sup->sup_asid;
    int pid = SYSCALL(GETPROCESSID, 0, 0, 0);

    // 1. Estrai il VPN completo che ha causato il fault
    unsigned int faulting_vpn = current_state->entry_hi >> VPNSHIFT;

    klog_print("vmSupport: Page Fault per ASID: ");
    klog_print_dec(asid);
    klog_print(", VPN (vero): ");
    klog_print_hex(faulting_vpn);
    klog_print("\n");

    // 2. Traduci il VPN nell'indice corretto per la page table
    unsigned int page_tbl_index;
    if (faulting_vpn == (USERSTACKTOP >> VPNSHIFT) - 1) {
        // Caso speciale per la pagina di stack, che è l'ultima della page table
        page_tbl_index = MAXPAGES - 1;
    } else {
        // Per tutte le altre pagine, l'indice è il VPN meno l'indirizzo di partenza
        page_tbl_index = faulting_vpn - (UPROCSTARTADDR >> VPNSHIFT);
    }

    // 3. Controlla che l'indice sia valido. Se non lo è, è un segmentation fault.
    if (page_tbl_index < 0 || page_tbl_index >= MAXPAGES) {
        klog_print("vmSupport: ERRORE! Accesso a memoria non valida (Segmentation Fault). Terminazione...\n");
        programTrapHandler(current_state);
        return; // Esci dal pager
    }

    // 4. Ottieni il puntatore corretto alla Page Table Entry usando l'indice calcolato
    pteEntry_t* pte_p = &sup->sup_privatePgTbl[page_tbl_index];

    // Adesso il resto del codice userà il puntatore corretto 'pte_p' e l'indice 'page_tbl_index'

    unsigned int cause = current_state->cause;
    if ((cause & CAUSE_EXCCODE_MASK) == EXC_MOD) {
        klog_print("vmSupport: ERRORE! Rilevata TLB-Modification exception. Terminazione...\n");
        programTrapHandler(current_state);
        return;
    };

    klog_print("vmSupport: Acquisizione mutex Swap Pool...\n");
    getMutex(&swap_pool_sem, pid);

    int victim = getSwapFrame();
    int page_out_needed = !isSwapFrameFree(victim);
    int x_asid = 0, k_vpn = 0;
    pteEntry_t* k_pte = NULL;

    if (page_out_needed) {
        x_asid = swap_pool_table[victim].sw_asid;
        k_vpn = swap_pool_table[victim].sw_pageNo;
        k_pte = swap_pool_table[victim].sw_pte;
    }

    klog_print("vmSupport: Rilascio mutex Swap Pool.\n");
    releaseMutex(&swap_pool_sem, pid);

    klog_print("vmSupport: Frame scelto per il rimpiazzo (vittima): ");
    klog_print_dec(victim);
    klog_print("\n");

    if (page_out_needed) {

        klog_print("vmSupport: Frame occupato da ASID: ");
        klog_print_dec(x_asid);
        klog_print(", VPN: ");
        klog_print_hex(k_vpn);
        klog_print(". Avvio scrittura su flash (page-out)...\n");

        CRITICAL_START();
        k_pte->pte_entryLO &= ~VALIDON;
        updateTLB(k_pte);
        read_or_write_flash(victim, k_vpn, x_asid, FLASHWRITE);
        klog_print("vmSupport: Page-out completato.\n");
        CRITICAL_END();

    }


    CRITICAL_START();
    klog_print("vmSupport: Avvio lettura da flash della nuova pagina (page-in)...\n");
    
    read_or_write_flash(victim, page_tbl_index, asid, FLASHREAD);
   
    klog_print("vmSupport: Page-in completato.\n");
    klog_print("vmSupport: Acquisizione mutex Swap Pool...\n");
    getMutex(&swap_pool_sem, pid);
    klog_print("vmSupport: Aggiornamento delle strutture dati (Swap Pool Table e Page Table)...\n");
    update_swap_pool_entry(victim, page_tbl_index, asid, pte_p);

    unsigned int pfn = (FRAMEPOOLSTART + (victim * PAGESIZE)) >> VPNSHIFT;

/* * CORREZIONE DEFINITIVA:
 * Costruiamo pte_entryLO da zero in un unico passaggio.
 * Questo è il modo corretto e robusto: combina il PFN (riportato a indirizzo base),
 * il bit di validità e il bit di "dirty".
 */
pte_p->pte_entryLO = (pfn << VPNSHIFT) | VALIDON | DIRTYON;

// (Qui puoi lasciare le tue stampe di debug per verificare)
  // --- BLOCCO DI DEBUG ---
  klog_print("  DEBUG: --- Valori prima di updateTLB ---\n");
  klog_print("  DEBUG: VPN (hex): ");
  klog_print_hex(ENTRYHI_GET_VPN(pte_p->pte_entryHI));
  klog_print(", ASID (dec): ");
  klog_print_dec(ENTRYHI_GET_ASID(pte_p->pte_entryHI));
  klog_print("\n");
  klog_print("  DEBUG: Frame fisico scelto (dec): ");
  klog_print_dec(victim);
  klog_print("\n");
  klog_print("  DEBUG: PFN calcolato (hex): ");
  klog_print_hex(pfn);
  klog_print("\n");
  klog_print("  DEBUG: pte_entryHI da scrivere (hex): ");
  klog_print_hex(pte_p->pte_entryHI);
  klog_print("\n");
  klog_print("  DEBUG: pte_entryLO da scrivere (hex): ");
  klog_print_hex(pte_p->pte_entryLO);
  klog_print("\n");
  klog_print("  DEBUG: --- Fine Blocco Debug ---\n");
  // --- FINE BLOCCO DI DEBUG ---

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