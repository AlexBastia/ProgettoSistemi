#include "headers/initial.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

extern void test();

int main() {
  // TODO: do I need to declare the Pass up vector?? Note: PASSUPVECTOR is a
  // constant with the memaddr off the PUV

  /* Populate Pass Up Vector */

  // passupvector->tlb_refll_handler = (memaddr)uTLB_RefillHandler; TODO: swap
  // with our own funcion

  /* Set the Stack Pointer for the Nucleus TLB-Refill event handler to the top
   * of the Nucleus stack page: 0x2000.1000 (constant KERNELSTACK) for CPU0 and
   * 0x20020000 + (cpu_id * PAGESIZE) for CPUs with ID greater or equal to 1.
   * Stacks in μRISC-V grow down. */
  /*TODO: cazzo vuol dire sta roba sopra??*/

  // passupvector->exception_handler = (memaddr)exceptionHandler; 
  // TODO: swap with our function

  /*Set the Stack pointer for the Nucleus exception handler to the top of the
Nucleus stack page: 0x2000.1000 (constant KERNELSTACK) for CPU0 and 0x20020000 +
(cpu_id * PAGESIZE) for CPUs with ID greater or equal to 1.*/
  /*TODO: stessa cosa, come si imposta lo stack pointer?*/

  /* Initialize Level 2 data structures */
  initPcbs();
  initASL();

  /* Initialize global variables (in header) */
  process_count = 0;
  for (int i = 0; i < UPROCMAX; i++)  // TODO: capire anche qui se NCPU o questo
    current_process[i] = NULL;
  global_lock = 0;
  // TODO: inizializza il resto e capisci se va bene cosi

  /* Load Interval Timer */
  // IntervalTimer.load(PSECOND) TODO: swappare con nostra funzione

  /* Instantiate first process */
  pcb_PTR pcb = allocPcb();
  pcb->p_s.mie = MIE_ALL;
  pcb->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
  // TODO: how do you set StackTop???
  pcb->p_parent = NULL;
  /* pcb->p_child = NULL; */  // TODO: questi non posso metteli a NULL perche'
                              // non sono puntatori...
  /* pcb->p_sib = NULL; */
  pcb->p_time = 0;
  pcb->p_semAdd = NULL;
  pcb->p_supportStruct = NULL;
  pcb->p_s.pc_epc = (memaddr)test;

  /*IRT*/
  // TODO: devo crearlo io?

  /* Set State for other CPUs */
  // TODO: da me dice che ne abbiamo solo 1...

  /* Call Scheduler */
  // TODO: aggiungi chiamata alla nostra funzione
}
