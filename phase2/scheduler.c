#include "headers/scheduler.h"

#include <uriscv/liburiscv.h>

#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/interrupts.h"

extern void klog_print(char*);

void Scheduler() {
  klog_print("Scheduler start");
  ACQUIRE_LOCK(&global_lock);
  klog_print("Scheduler lock acquired");
  if (emptyProcQ(&ready_queue)) {
    klog_print("Scheduler empty queue");
    if (process_count == 0) {
      klog_print("Scheduler process count 0");
      RELEASE_LOCK(&global_lock);
      HALT();
    } else {
      klog_print("Scheduler process count > 0");
      // non ho ben capito, ma credo che il processo debba aspettare un interrupt PS: l'ha capito

      //! capire cosa cuazzo è sta roba (l'ha scritta il prof, io non c'entro )
      RELEASE_LOCK(&global_lock);
      unsigned int status = getSTATUS();
      klog_print("Scheduler get status");
      status |= MSTATUS_MIE_MASK;
      setSTATUS(status);
      klog_print("Scheduler set status");
      setMIE(MIE_ALL & ~MIE_MTIE_MASK);
      klog_print("Scheduler set mie");
      setTIMER(TIMESLICE);
      klog_print("Scheduler set timer");

      klog_print("Scheduler wait");
      *((memaddr*)TPR) = 1;

      WAIT();  // attesa di un interrupt

      // see Dott. Rovelli’s thesis for more details.
    }
  } else {
    klog_print("Scheduler not empty queue");
    pcb_t* next = removeProcQ(&ready_queue);
    current_process[getPRID()] = next;
    setTIMER(TIMESLICE);
    *((memaddr*)TPR) = 0;
    RELEASE_LOCK(&global_lock);
    LDST(&(next->p_s));  // context switch al nuovo processo
  }
}
