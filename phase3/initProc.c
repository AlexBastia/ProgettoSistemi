#include "headers/initProc.h"

#include <time.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

swap_t swap_pool_table[POOLSIZE];
supSem swap_pool_sem;
supSem sharable_dev_sem[NSUPPSEM];
int masterSemaphore;

void test() {
  /* Initialize Level 4 Data Structures */

  // Swap Pool table
  for (int i = 0; i < 2 * UPROCMAX; i++) {
    swap_pool_table[i].sw_asid = -1;  // Unoccupied
    swap_pool_table[i].sw_pageNo = -1;
    swap_pool_table[i].sw_pte = NULL;
  }

  // Swap Pool semaphore
  swap_pool_sem.value = 1;

  // Device mutex semaphores (Note: these are not actually needed for this phase, so double check they actually work!)
  for (int i = 0; i < NSUPPSEM; i++) sharable_dev_sem[i].value = 1;

  /* Initialize and launch U-procs */
  // Set initial processor state
  // Domande:
  // 1: Che valore devono avere status e mie per essere in modalita' utente con interrupt e PLT attivati?
  // 2: Devo inizializzare sempre il massimo numero di processi? Poi quanti ne devo eseguire?
  state_t initial_states[UPROCMAX];
  for (int i = 0; i < UPROCMAX; i++) {
    initial_states[i].pc_epc = UPROCSTARTADDR;
    initial_states[i].reg_sp = USERSTACKTOP;
    initial_states[i].status = MSTATUS_MPIE_MASK;
    initial_states[i].mie = MIE_ALL;
    initial_states[i].entry_hi = i + 1;
  }

  // Set support structure
  support_t supports[UPROCMAX];
  for (int i = 0; i < UPROCMAX; i++) {
    supports[i].sup_asid = i + 1;
    // supports[i].sup_exceptContext[PGFAULTEXCEPT].pc = TLB_Handler; TODO: usare handler nostri
    // supports[i].sup_exceptContext[GENERALEXCEPT].pc = GenExcept_Handler
    supports[i].sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPP_M;  // TODO: Spero che sta roba lo metti in kernel mode, da controllare
    supports[i].sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPP_M;
    supports[i].sup_exceptContext[PGFAULTEXCEPT].stackPtr = (unsigned int)&(supports[i].sup_stackTLB[499]);
    supports[i].sup_exceptContext[GENERALEXCEPT].stackPtr = (unsigned int)&(supports[i].sup_stackTLB[499]);
  }

  // Launch
  masterSemaphore = 0;
  for (int i = 0; i < UPROC_NUM; i++) {
    SYSCALL(CREATEPROCESS, (int)&initial_states[i], PROCESS_PRIO_LOW, (int)&supports[i]);
  }

  /* Termination */
  for (int i = 0; i < UPROC_NUM; i++) {
    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0);  // Each Uproc must call NSYS4 on the masterSemaphore before terminating
  }
  SYSCALL(TERMPROCESS, 0, 0, 0);  // HALT
}

void getMutex(supSem* sem, int pid) {
  // wait for the semaphore to be free
  SYSCALL(PASSEREN, (int)&sem->value, 0, 0);
  // change mutex holder
  sem->holder_pid = pid;
  // Note: ci potrebbero essere problemi se non e' atomica sta roba??
}

void releaseMutex(supSem* sem, int pid) {
  // check if pid is the mutex holder
  if (pid != sem->holder_pid) return;
  // release mutex
  SYSCALL(VERHOGEN, (int)&sem->value, 0, 0);
  // change mutex holder to -1
  sem->holder_pid = -1;
}

void releaseAllMutex(int pid) {
  // for each semaphore call release mutex
  for (int i = 0; i < NSUPPSEM; i++) releaseMutex(&sharable_dev_sem, pid);
  releaseMutex(&swap_pool_sem, pid);
}
