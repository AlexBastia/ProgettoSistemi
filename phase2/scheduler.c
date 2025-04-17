#include "headers/scheduler.h"

#include <uriscv/liburiscv.h>

#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/interrupts.h"

extern void klog_print(char*);
extern void klog_print_dec(int);
void Scheduler() {
  ACQUIRE_LOCK(&global_lock);
  if (emptyProcQ(&ready_queue)) {
    klog_print("Scheduler empty queue");
    if (process_count == 0) {
      /* No more processes */
      klog_print("Scheduler process count 0");
      RELEASE_LOCK(&global_lock);
      HALT();
    } else {
      /* Wait for interrupt */
      klog_print("Scheduler process count > 0");
      // non ho ben capito, ma credo che il processo debba aspettare un interrupt PS: l'ha capito

      //! capire cosa cuazzo è sta roba (l'ha scritta il prof, io non c'entro )
      RELEASE_LOCK(&global_lock);
      setMIE(MIE_ALL & ~MIE_MTIE_MASK);
      unsigned int status = getSTATUS();
      status |= MSTATUS_MIE_MASK;
      setSTATUS(status);

      *((memaddr*)TPR) = 1;
      WAIT();  // attesa di un interrupt

      // see Dott. Rovelli’s thesis for more details.
    }
  } else {
    /* Run process */
    klog_print("Scheduler not empty queue");
    pcb_t* next = removeProcQ(&ready_queue);
    current_process[getPRID()] = next;
    setTIMER(TIMESLICE);
    *((memaddr*)TPR) = 0;
    STCK(proc_time_started[getPRID()]);  // set the time of the current process
    RELEASE_LOCK(&global_lock);
    klog_print("current pid:");
    klog_print_dec(next->p_pid);
    LDST(&(next->p_s));  // context switch al nuovo processo
  }
}
