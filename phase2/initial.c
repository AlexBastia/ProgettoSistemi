#include "headers/initial.h"

#include <uriscv/arch.h>
#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/exceptions.h"
#include "headers/scheduler.h"

extern void test();
extern void uTLB_RefillHandler();

/* Declaration of global variables */
int process_count;
struct list_head ready_queue;
pcb_PTR current_process[NCPU];
int device_semaphores[SEMDEVLEN];
volatile unsigned int global_lock = 1;
cpu_t proc_time_started[NCPU];

int main() {
  /* Populate Pass Up Vector */
  passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
  for (int i = 0; i < NCPU; i++) {
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = (i == 0) ? KERNELSTACK : RAMSTART + (64 * PAGESIZE) + (i * PAGESIZE);
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = (i == 0) ? KERNELSTACK : 0x20020000 + (i * PAGESIZE);
    passupvector++;
  }

  /* Initialize Level 2 data structures */
  initPcbs();
  initASL();

  /* Initialize global variables (in header) */
  process_count = 0;
  INIT_LIST_HEAD(&ready_queue);
  for (int i = 0; i < NCPU; i++) current_process[i] = NULL;
  global_lock = 0;
  for (int i = 0; i < SEMDEVLEN; i++) {
    device_semaphores[i] = 0;
  }

  /* Load Interval Timer */
  LDIT(PSECOND);

  /* Instantiate first process */
  pcb_PTR pcb = allocPcb();
  RAMTOP(pcb->p_s.reg_sp);  // Set SP to last RAM frame

  // Enable interrupts and kernel mode
  pcb->p_s.mie = MIE_ALL;
  pcb->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;

  pcb->p_s.pc_epc = (memaddr)test;
  insertProcQ(&ready_queue, pcb);
  process_count++;

  /*IRT*/
  for (int line = 0; line < N_INTERRUPT_LINES; line++) {  // For each Interrupt Line
    for (int dev = 0; dev < N_DEV_PER_IL; dev++) {        // For each Device in Interrupt Line
      int *irt_entry = (int *)IRT_ENTRY(line, dev);
      *irt_entry = IRT_RP_BIT_ON;                                                  // Set RP bit to 1
      for (int cpu_id = 0; cpu_id < NCPU; cpu_id++) *irt_entry |= (1U << cpu_id);  // Set bit at index cpu_id (starting from least significant) to 1 for each CPU
    }
  }

  /* Set State for other CPUs */
  state_t initial_states[NCPU - 1];
  for (int i = 0; i < NCPU - 1; i++) {
    STST(&initial_states[i]);
    initial_states[i].status = MSTATUS_MPP_M;
    initial_states[i].pc_epc = (memaddr)Scheduler;
    initial_states[i].reg_sp = 0x20020000 + ((i + 1) * PAGESIZE);
    initial_states[i].entry_hi = 0;
    initial_states[i].cause = 0;
    initial_states[i].mie = 0;
    INITCPU(i + 1, &initial_states[i]);
  }

  /* Call Scheduler */
  Scheduler();
}
