#include "headers/initial.h"

#include <uriscv/types.h>

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/scheduler.h"

extern void test();

int main() {
  /* Populate Pass Up Vector */
  for (int i = 0; i < NCPU; i++) {
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR + (0x10 * i);
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr =
        (i == 0) ? KERNELSTACK : 0x20020000 + (i * PAGESIZE);
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = passupvector->tlb_refill_stackPtr;
  }

  /* Initialize Level 2 data structures */
  initPcbs();
  initASL();

  /* Initialize global variables (in header) */
  process_count = 0;
  INIT_LIST_HEAD(&ready_queue);
  for (int i = 0; i < NCPU; i++) current_process[i] = NULL;
  global_lock = 0;
  /* TODO: init device semaphores */

  /* Load Interval Timer */
  // IntervalTimer.load(PSECOND) TODO: swappare con nostra funzione

  /* Instantiate first process */
  pcb_PTR pcb = allocPcb();
  pcb->p_s.mie = MIE_ALL;
  pcb->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
  // TODO: how do you set StackTop???
  pcb->p_parent = NULL;
  INIT_LIST_HEAD(&pcb->p_sib);
  INIT_LIST_HEAD(&pcb->p_child);
  pcb->p_time = 0;
  pcb->p_semAdd = NULL;
  pcb->p_supportStruct = NULL;
  pcb->p_s.pc_epc = (memaddr)test;

  list_add_tail(&ready_queue, &pcb->p_list); /* TODO: aggiunto in coda per
                                                evitare starvation, va bene? */
  process_count++;

  /*IRT*/
  // TODO: devo crearlo io?

  /* Set State for other CPUs */
  for (int cpu_id = 1; cpu_id < NCPU; cpu_id++) {
    state_t *processor_state = (state_t *)BIOSDATAPAGE + (cpu_id * 0x94);
    processor_state->status = MSTATUS_MPP_M;
    processor_state->pc_epc = (memaddr)Scheduler;
    processor_state->reg_sp = 0x20020000 + (cpu_id * PAGESIZE);
    processor_state->entry_hi = 0;
    processor_state->cause = 0;
    processor_state->mie = 0;
  }
  /* TODO: come mai non lo devo fare per il primo CPU? */

  /* Call Scheduler */
  Scheduler();
}
