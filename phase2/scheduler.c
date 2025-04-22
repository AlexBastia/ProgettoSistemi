#include "headers/scheduler.h"

#include <uriscv/liburiscv.h>

#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/interrupts.h"

void Scheduler() {
  ACQUIRE_LOCK(&global_lock);
  if (emptyProcQ(&ready_queue)) {
    if (process_count == 0) {
      /* No more processes */
      RELEASE_LOCK(&global_lock);
      HALT();
    } else {
      /* Wait for interrupt */
      RELEASE_LOCK(&global_lock);
      setMIE(MIE_ALL & ~MIE_MTIE_MASK);
      unsigned int status = getSTATUS();
      status |= MSTATUS_MIE_MASK;
      setSTATUS(status);

      *((memaddr*)TPR) = 1; //set highest priority
      WAIT();
    }
  } else {
    /* Run ready process */
    pcb_t* next = removeProcQ(&ready_queue);
    current_process[getPRID()] = next;
    setTIMER(TIMESLICE);  // initialise PLT

    *((memaddr*)TPR) = 0;

    STCK(proc_time_started[getPRID()]);  // set the start time of the current process
    RELEASE_LOCK(&global_lock);
    LDST(&(next->p_s));  // context switch to next process
  }
}
