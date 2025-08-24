#include "headers/initProc.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "headers/sysSupport.h"
#include "headers/vmSupport.h"

extern void klog_print(char* str);
extern void klog_print_dec(int num);

// Dichiarazioni delle variabili globali
swap_t swap_pool_table[POOLSIZE];
supSem swap_pool_sem;
supSem sharable_dev_sem[NSUPPSEM];
int masterSemaphore;

void test() {
  klog_print("test: --- Inizio del processo di inizializzazione (test) ---\n");

  // Inizializzazione delle strutture dati della Fase 3
  klog_print("test: Inizializzazione della Swap Pool Table...\n");
  for (int i = 0; i < POOLSIZE; i++) {
    swap_pool_table[i].sw_asid = -1;
    swap_pool_table[i].sw_pageNo = -1;
    swap_pool_table[i].sw_pte = NULL;
  }
  klog_print("test: Inizializzazione dei semafori di supporto...\n");
  swap_pool_sem.value = 1;
  swap_pool_sem.holder_pid = -1;
  for (int i = 0; i < NSUPPSEM; i++) {
    sharable_dev_sem[i].value = 1;
    sharable_dev_sem[i].holder_pid = -1;
  }
  masterSemaphore = 0;
  klog_print("test: Strutture dati della Fase 3 inizializzate con successo.\n");

  // Preparazione degli U-proc
  state_t initial_states[UPROCMAX];
  support_t supports[UPROCMAX];
  klog_print("test: Inizio del ciclo di creazione degli U-proc...\n");

  for (int i = 0; i < UPROC_NUM; i++) {
    int asid = i + 1;
    klog_print("test: Configurazione per U-proc con ASID: ");
    klog_print_dec(asid);
    klog_print("\n");

    // Impostazione dello stato iniziale del processore
    initial_states[i].pc_epc = UPROCSTARTADDR;
    initial_states[i].reg_sp = USERSTACKTOP;
    initial_states[i].status = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK;
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
    klog_print("test: SYSCALL(CREATEPROCESS) eseguita per ASID: ");
    klog_print_dec(asid);
    klog_print("\n");
  }

  klog_print("test: Tutti gli U-proc sono stati creati. In attesa della loro terminazione...\n");

  // Attesa della terminazione di tutti gli U-proc
  for (int i = 0; i < UPROC_NUM; i++) {
    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0);
    klog_print("test: Rilevata la terminazione di un U-proc. Processi rimanenti: ");
    klog_print_dec(UPROC_NUM - (i + 1));
    klog_print("\n");
  }

  klog_print("test: --- Tutti gli U-proc sono terminati. Terminazione del sistema. ---\n");
  SYSCALL(TERMPROCESS, 0, 0, 0);
}

void getMutex(supSem* sem, int pid) {
  klog_print("mutex: Processo ");
  klog_print_dec(pid);
  klog_print(" tenta di acquisire un mutex.\n");
  SYSCALL(PASSEREN, (int)&sem->value, 0, 0);
  sem->holder_pid = pid;
  klog_print("mutex: Processo ");
  klog_print_dec(pid);
  klog_print(" ha acquisito il mutex.\n");
}

void releaseMutex(supSem* sem, int pid) {
  if (pid != sem->holder_pid) return;
  SYSCALL(VERHOGEN, (int)&sem->value, 0, 0);
  sem->holder_pid = -1;
  klog_print("mutex: Processo ");
  klog_print_dec(pid);
  klog_print(" ha rilasciato un mutex.\n");
}

void releaseAllMutex(int pid) {
  klog_print("mutex: Rilascio di tutti i mutex per il PID: ");
  klog_print_dec(pid);
  klog_print("\n");
  for (int i = 0; i < NSUPPSEM; i++) {
    releaseMutex(&sharable_dev_sem[i], pid);
  }
  releaseMutex(&swap_pool_sem, pid);
}
