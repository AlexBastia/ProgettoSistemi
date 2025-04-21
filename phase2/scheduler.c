#include "headers/scheduler.h"

#include <uriscv/liburiscv.h>

#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/interrupts.h"

extern void klog_print(char*);
extern void klog_print_dec(int);
extern void klog_print_hex(int);
void Scheduler() {
  klog_print("Scheduler");
  ACQUIRE_LOCK(&global_lock);
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

      *((memaddr*)TPR) = 1;
      klog_print("-end ");
      WAIT();  // attesa di un interrupt

      // see Dott. Rovelli’s thesis for more details.
    }
  } else {
    /* Run process */
    pcb_t* next = removeProcQ(&ready_queue);
    current_process[getPRID()] = next;
    setTIMER(TIMESLICE);  // initialise PLT

    *((memaddr*)TPR) = 0;  // TODO: se ogni processore deve avere una pririta' diversa, perche'
                           // c'e' solo un indirizzo fisso? Nelle specifiche dice di fare
                           // questo ma giuro che non ha senso come cosa

    STCK(proc_time_started[getPRID()]);  // set the time of the current process
    RELEASE_LOCK(&global_lock);
    klog_print("-run:");
    klog_print_dec(next->p_pid);
    klog_print("-at:");
    klog_print_hex(next->p_s.pc_epc);
    klog_print("-end ");
    LDST(&(next->p_s));  // context switch al nuovo processo
  }
}
