#include "headers/initProc.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "headers/sysSupport.h"
#include "headers/vmSupport.h"

#define MSTATUS_FS_INITIAL (1 << 13)
// Dichiarazioni delle variabili globali
swap_t swap_pool_table[POOLSIZE];
supSem swap_pool_sem;
supSem sharable_dev_sem[NSUPPSEM];
int masterSemaphore;

void test() {

  // Inizializzazione delle strutture dati della Fase 3
  for (int i = 0; i < POOLSIZE; i++) {
    swap_pool_table[i].sw_asid = -1;
    swap_pool_table[i].sw_pageNo = -1;
    swap_pool_table[i].sw_pte = NULL;
  }
  swap_pool_sem.value = 1;
  swap_pool_sem.holder_pid = -1;
  for (int i = 0; i < NSUPPSEM; i++) {
    sharable_dev_sem[i].value = 1;
    sharable_dev_sem[i].holder_pid = -1;
  }
  masterSemaphore = 0;

  // Preparazione degli U-proc
  state_t initial_states[UPROCMAX];
  support_t supports[UPROCMAX];

  for (int i = 0; i < UPROC_NUM; i++) {
    int asid = i + 1;

    // Impostazione dello stato iniziale del processore
    initial_states[i].pc_epc = UPROCSTARTADDR;
    initial_states[i].reg_sp = USERSTACKTOP;
    initial_states[i].status = MSTATUS_MPIE_MASK;
    initial_states[i].mie = MIE_ALL;
    initial_states[i].entry_hi = (unsigned int)asid << ASIDSHIFT;

    // Inizializzazione della Struttura di Supporto
    supports[i].sup_asid = asid;
    supports[i].sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;
    supports[i].sup_exceptContext[PGFAULTEXCEPT].stackPtr = (unsigned int)&(supports[i].sup_stackTLB[499]);
    supports[i].sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPP_M;

    supports[i].sup_exceptContext[GENERALEXCEPT].pc = (memaddr)generalExceptionSupportHandler;
    supports[i].sup_exceptContext[GENERALEXCEPT].stackPtr = (unsigned int)&(supports[i].sup_stackGen[499]);
    supports[i].sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPP_M;

    // Inizializzazione della Page Table
    for (int j = 0; j < MAXPAGES; j++) {
      unsigned int vpn = (j < MAXPAGES - 1) ? (0x80000 + j) : 0xBFFFF;
      supports[i].sup_privatePgTbl[j].pte_entryHI = (vpn << VPNSHIFT) | (asid << ASIDSHIFT);
      supports[i].sup_privatePgTbl[j].pte_entryLO = DIRTYON;
    }

    // Creazione del processo
    SYSCALL(CREATEPROCESS, (int)&initial_states[i], PROCESS_PRIO_LOW, (int)&supports[i]);
  }


  // Attesa della terminazione di tutti gli U-proc
  for (int i = 0; i < UPROC_NUM; i++) {
    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0);
  }

  SYSCALL(TERMPROCESS, 0, 0, 0);
}

void getMutex(supSem* sem, int pid) {
  SYSCALL(PASSEREN, (int)&sem->value, 0, 0);
  sem->holder_pid = pid;
}

void releaseMutex(supSem* sem, int pid) {
  if (pid != sem->holder_pid) return;
  SYSCALL(VERHOGEN, (int)&sem->value, 0, 0);
  sem->holder_pid = -1;
}

void releaseAllMutex(int pid) {
  for (int i = 0; i < NSUPPSEM; i++) {
    releaseMutex(&sharable_dev_sem[i], pid);
  }
  releaseMutex(&swap_pool_sem, pid);
}
