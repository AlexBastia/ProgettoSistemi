#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../headers/types.h"

#define SHARABLE_DEV_NUM 48
#define UPROC_NUM 8

swap_t swap_pool_table[2 * UPROCMAX];
int swap_pool_sem;
int sharable_dev_sem[SHARABLE_DEV_NUM];
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
  swap_pool_sem = 1;

  // Device mutex semaphores (Note: these are not actually needed for this phase, so double check they actually work!)
  // Domande:
  // 1: Non so quanti device sono "potentialy sharable" quindi li metto tutti, probabilmente da cambiare
  // -> ci sono un terminale (due sub-device), una stampante e un flash device per U-proc, quindi forse 4*8=32 solo che potrebbe creare casini per l'indicizzazione nella parte prima (forse)
  for (int i = 0; i < SHARABLE_DEV_NUM; i++) sharable_dev_sem[i] = 1;

  /* Initialize and launch U-procs */
  // Set initial processor state
  // Domande:
  // 1: Che valore devono avere status e mie per essere in modalita' utente con interrupt e PLT attivati?
  state_t initial_states[UPROC_NUM];
  for (int i = 0; i < UPROC_NUM; i++) {
    initial_states[i].pc_epc = UPROCSTARTADDR;
    initial_states[i].reg_sp = USERSTACKTOP;
    initial_states[i].status = MSTATUS_MPIE_MASK;
    initial_states[i].mie = MIE_ALL;
    initial_states[i].entry_hi = i + 1;
  }

  // Set support structure
  support_t supports[UPROC_NUM];
  for (int i = 0; i < UPROC_NUM; i++) {
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
    SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);  // Each Uproc must call NSYS4 on the masterSemaphore before terminating
  }
  SYSCALL(TERMPROCESS, 0, 0, 0);  // HALT
}
