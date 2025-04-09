#include "headers/initial.h"

#include <uriscv/arch.h>
#include <uriscv/const.h>
#include <uriscv/types.h>

#include "../klog.c"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/exceptions.h"
#include "headers/scheduler.h"

extern void test();

/* Declaration of global variables */
int process_count;
struct list_head ready_queue;
pcb_PTR current_process[NCPU];
int device_semaphores[SEMDEVLEN];
volatile unsigned int global_lock = 1; /* 0 or 1 */

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
  pcb->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M;

  // Set Process Tree fields to NULL
  pcb->p_parent = NULL;
  INIT_LIST_HEAD(&pcb->p_sib); /* TODO: can't set p_sib and p_child to NULL, do
                                  I have to initialize? */
  INIT_LIST_HEAD(&pcb->p_child);

  pcb->p_time = 0;
  pcb->p_semAdd = NULL;
  pcb->p_supportStruct = NULL;
  pcb->p_s.pc_epc = (memaddr)test;

  list_add_tail(&ready_queue, &pcb->p_list); /* TODO: aggiunto in coda per
                                                evitare starvation, va bene? */
  process_count++;

  /*IRT*/
  for (int line = 0; line < N_INTERRUPT_LINES;
       line++) {  // For each Interrupt Line
    for (int dev = 0; dev < N_DEV_PER_IL;
         dev++) {  // For each Device in Interrupt Line
      int *irt_entry = (int *)IRT_ENTRY(line, dev);
      *irt_entry |= (1U << IRT_ENTRY_POLICY_BIT);  // Set RP bit to 1
      for (int cpu_id = 0; cpu_id < NCPU; cpu_id++)
        *irt_entry |= (1U << cpu_id);  // Set bit at index cpu_id (starting from
                                       // least significant) to 1 for each CPU
    }
  }

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
  klog_print("finito inital.c");
  /* Call Scheduler */
  Scheduler();
}
