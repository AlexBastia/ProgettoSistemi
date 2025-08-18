#include "headers/vmSupport.h"

/*
To access the Swap Pool table, a process must first perform a NSYS3 (P) operation on this
semaphore. When access to the Swap Pool table is concluded, a process will then perform a NSYS4
(V) operation on this semaphore. Since this semaphore is used for mutual exclusion, it should be
initialized to one*/
volatile unsigned int swap_pool_sem = 1;
//idk questo mi era necessario esistesse gia` ora, poi inizializzalo
swap_t swap_pool[POOLSIZE];
/*Important: Using the μRISC-V Machine Configuration Panel make sure that there is sufficient
“installed” RAM for the OS code, the Swap Pool and stack page for test (e.g. 512 frames).*/
#define SWAPPOOLSTART RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE);
/* Macro per aprire una critical section */
#define CRITICAL_START() unsigned int _cs_status = getSTATUS(); setSTATUS(_cs_status & DISABLEINTS)

/* Macro per chiudere una critical section */
#define CRITICAL_END() setSTATUS(_cs_status)



//CHIAMARE SEMPRE IN UNA CRITICAL SECTION
void updateTLB(pteEntry_t* p){
    /* Strategia semplice: TLBCLR, poi scrivi la PTE corrente */
    TLBCLR();
    setENTRYHI(p->pte_entryHI);
    setENTRYLO(p->pte_entryLO);
    TLBWR();
};

//IMPORTANT TODO AFTER DEBUGGING : cambiare ogni istanza di updateTLB() con updateTLBv2(). non so bene perche` ma nel pdf dicono di finire il prog. prima di usare questa [5.2]
void updateTLB_v2(pteEntry_t* p) {
    setENTRYHI(p->pte_entryHI);
    TLBP();
    if (!(getINDEX() & PRESENTFLAG)) { // Index.P == 0 significa entry presente
        setENTRYLO(p->pte_entryLO);
        TLBWI();
    }
}

int isSwapFrameOccupied(int frame) {
    return swap_pool[frame].sw_asid != 0; // 0 indica frame libero
}
int getNextSwapFrame() {
    static int nextFrame = 0;      // variabile statica per FIFO
    int frame = nextFrame;
    nextFrame = (nextFrame + 1) % POOLSIZE;  // round-robin
    return frame;
}

   /* Funzione generica per leggere o scrivere un frame sulla flash */
void read_or_write_flash(int frame_i, int vpn, int asid, int op) {
    unsigned int frame_phys = FRAMEPOOLSTART + (frame_i * PAGESIZE);
    volatile dtpreg_t *flash = (dtpreg_t*) DEV_REG_ADDR(IL_FLASH, asid - 1);
    flash->data0 = frame_phys;
    unsigned int cmd = ((unsigned int)vpn << 8) | op;
    int io = SYSCALL(DOIO, flash->command, cmd, 0);
    if (io != OK) {
        TRAP;
    }
}
/* Aggiorna la swap pool dopo aver letto/scritto un frame */
void update_swap_pool_entry(int frame_i, int vpn, int asid, pteEntry_t *pte) {
    swap_pool[frame_i].sw_pageNo = vpn;   /* pagina logica che ora occupa il frame */
    swap_pool[frame_i].sw_asid   = asid;  /* ASID del processo proprietario */
    swap_pool[frame_i].sw_pte    = pte;   /* puntatore alla page table entry */
}





//pdf 4.2
int pager(){
    //punto1
    support_t* sup = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    //punto2
    state_t* current_state = &sup->sup_exceptState[PGFAULTEXCEPT];
    unsigned int cause = current_state->cause;
    //punto3
    if((cause&CAUSE_EXCCODE_MASK) == EXC_MOD){
        TRAP; //TODO i guess
    };
    //punto4
    SYSCALL(PASSEREN, &swap_pool_sem, 0, 0);
    //punto5
    unsigned int p = ENTRYHI_GET_VPN(current_state->entry_hi);//macro da cpu.h
    unsigned int asid = sup->sup_asid;
    pteEntry_t* pte_p = &sup->sup_privatePgTbl[p];

    //punto6
    for (int i = 0; i < POOLSIZE; i++){
        if(swap_pool[i].sw_asid==asid && swap_pool[i].sw_pageNo == p){
            CRITICAL_START();
            updateTLB(pte_p);
            CRITICAL_END();
            //6c: (se la pagina e` nella swap pool, e` valida per forza)
            SYSCALL(VERHOGEN, &swap_pool_sem, 0, 0);
            LDST(current_state);
        }
    }
    //punto 7
    int victim = getNextSwapFrame();
    unsigned int frame_phys = FRAMEPOOLSTART + (victim * PAGESIZE);

    //punto 9
    if(isSwapFrameOccupied(victim)){ //la funzione e` il punto 8
        int          x_asid = swap_pool[victim].sw_asid;
        unsigned int k_vpn  = swap_pool[victim].sw_pageNo;
        pteEntry_t*  k_pte  = swap_pool[victim].sw_pte;
        CRITICAL_START();
        //9a
        k_pte->pte_entryLO &= ~ENTRYLO_VALID; //not valid page
        //9b
        updateTLB(k_pte);
        CRITICAL_END();
        //9c
        read_or_write_flash(victim, k_vpn, x_asid, FLASHWRITE);
    }
    //punto10
    read_or_write_flash(victim, p, asid, FLASHREAD);
    //punto11
    update_swap_pool_entry(victim, p, asid, pte_p);
    //punto12
    CRITICAL_START();
    pte_p->pte_entryLO |= ENTRYLO_VALID;    //pagina valida
    pte_p->pte_entryLO &= ~ENTRYLO_PFN_MASK; //cancella vecchio pfn
    pte_p->pte_entryLO |= (victim << ENTRYLO_PFN_BIT); //scrivi nuovo pfn
    //punto13
    updateTLB(pte_p);
    CRITICAL_END();
    //14 
    SYSCALL(VERHOGEN, &swap_pool_sem, 0, 0);
    //15
    LDST(current_state);
}