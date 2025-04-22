#include "headers/scheduler.h"

#include <uriscv/liburiscv.h>

#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/interrupts.h"

extern void klog_print(char*);
extern void klog_print_dec(int);

void Scheduler() {
  ACQUIRE_LOCK(&global_lock);
  klog_print("Scheduler");
  if (emptyProcQ(&ready_queue)) {
    if (process_count == 0) {
      /* No more processes */
      klog_print("-pc=0-halt");
      RELEASE_LOCK(&global_lock);
      HALT();
    } else {
      /* Wait for interrupt */
      klog_print("-pc>0 ");
      RELEASE_LOCK(&global_lock);
      setMIE(MIE_ALL & ~MIE_MTIE_MASK);
      unsigned int status = getSTATUS();
      status |= MSTATUS_MIE_MASK;
      setSTATUS(status);

      *((memaddr*)TPR) = 1; //set highest priority
      WAIT();
    }
  } else {
    /* Run process */
    pcb_t* next = removeProcQ(&ready_queue);
    current_process[getPRID()] = next;
    setTIMER(TIMESLICE);  // initialise PLT

    *((memaddr*)TPR) = 0; 

    STCK(proc_time_started[getPRID()]);  // set the start time of the current process
    klog_print("-run:");
    klog_print_dec(next->p_pid);
    RELEASE_LOCK(&global_lock);
    LDST(&(next->p_s));  // context switch to the next process
  }
}
