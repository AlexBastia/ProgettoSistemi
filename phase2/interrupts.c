#include "headers/interrupts.h"

#include <uriscv/liburiscv.h>

#include "headers/initial.h"
#define SAVE_STATE(x) (current_process[x]->p_s = *GET_EXCEPTION_STATE_PTR(x))

extern void klog_print(char *);
extern void klog_print_dec(int);
extern void klog_print_hex(int);

void interruptHandler(state_t *current_state) {
  // cpu_t end_time;
  // STCK(end_time);  // get the current time
  // current_process[getPRID()]->p_time += end_time - proc_time_started[getPRID()];  // update the time of the current process
  klog_print("-interrupt");
  unsigned int int_code = getCAUSE() & CAUSE_EXCCODE_MASK;  // trova il device che ha causato l'interrupt
  int intlineNo = getintLineNo(int_code);

  switch (intlineNo) {
    case 1:
      pltHandler(current_state);
      break;
    case 2:
      timerHandler(current_state);
      break;
    case 3 ... 7:
      intHandler(intlineNo, current_state);
      break;
    default:
      break;
  }
}

// trova quale tipo di device ha causato l'interrupt dall'int_code
int getintLineNo(int int_code) {
  switch (int_code) {
    case IL_CPUTIMER:
      return 1;
    case IL_TIMER:
      return 2;
    case IL_DISK:
      return 3;
    case IL_FLASH:
      return 4;
    case IL_ETHERNET:
      return 5;
    case IL_PRINTER:
      return 6;
    case IL_TERMINAL:
      return 7;
    default:
      return -1;
  }
}

// trova, sapendo il tipo del device interrompente, quale degli 8 device di esso ha causato l'interrupt
int getdevNo(int intlineNo) {
  unsigned int bitmapValue = *((unsigned int *)(INTERRUPT_BITMAP_ADDRESS + (intlineNo - 3) * 4));  // va alla parola giusta della bitmap
  if (bitmapValue & DEV0ON) return 0;
  if (bitmapValue & DEV1ON) return 1;
  if (bitmapValue & DEV2ON) return 2;
  if (bitmapValue & DEV3ON) return 3;
  if (bitmapValue & DEV4ON) return 4;
  if (bitmapValue & DEV5ON) return 5;
  if (bitmapValue & DEV6ON) return 6;
  if (bitmapValue & DEV7ON) return 7;
  return -1;
}

// gestisce gli interrupt da plt
void pltHandler(state_t *current_state) {
  klog_print("-PLT");
  setTIMER(TIMESLICE);  // Acknowledge the PLT interrupt by loading the timer with a new value using setTIMER.
  current_process[getPRID()]->p_s = *current_state;
  // Copy the processor state of the current CPU at the time of the exception into the Current Process’s PCB (p_s) of the current CPU
  ACQUIRE_LOCK(&global_lock);
  insertProcQ(&ready_queue, current_process[getPRID()]);  // Place the Current Process on the Ready Queue;
  RELEASE_LOCK(&global_lock);
  klog_print("-end ");
  Scheduler();
}

void UNBLOCKALLWAITINGCLOCKPCBS() {
  pcb_t *pcb = NULL;
  ACQUIRE_LOCK(&global_lock);
  while ((pcb = removeBlocked(&device_semaphores[PSEUDOCLOCK])) != NULL) {
    insertProcQ(&ready_queue, pcb);
  }
  RELEASE_LOCK(&global_lock);
}

// gestisce gli interrupt da pseudoclock
void timerHandler(state_t *current_state) {
  klog_print("-timer");
  LDIT(PSECOND);                 // Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
  UNBLOCKALLWAITINGCLOCKPCBS();  // Unblock all PCBs blocked waiting a Pseudo-clock tick.
  ACQUIRE_LOCK(&global_lock);
  pcb_PTR curr = current_process[getPRID()];
  RELEASE_LOCK(&global_lock);
  if (curr != NULL) {
    klog_print("-loadingCurr ");
    LDST(current_state);  // Return control to the Current Process of the current CPU if exists: perform a LDST on the saved exception state of the current CPU.
  }
  // it is also possible that there is no Current Process to return control to. This will be the case when the Scheduler executes the WAIT instruction instead of dispatching a process for execution
  klog_print("-end ");
  Scheduler();
}
void intHandler(int intlineNo, state_t *current_state) {
  klog_print("-int");
  int devNo = getdevNo(intlineNo);
  memaddr devAddrBase = START_DEVREG + ((intlineNo - 3) * 0x80) + (devNo * 0x10);  // punto 1
  // come sacrosanta minchia si fa a capire quale dei due subdevice del terminal (con lo schema alla fine del pdf, in device registers) causa l'interrupt????
  if (intlineNo == 7) {
    unsigned int *writestatusaddress = (unsigned int *)devAddrBase + 0x8;
    int TERMINALWRITEINTERRUPT = *writestatusaddress;  // si spera che questo funzioni
    if (TERMINALWRITEINTERRUPT != 0) {
      klog_print("-write");
      devAddrBase += 0x8;
    }
  }

  unsigned int status = *(unsigned int *)devAddrBase;       // punto 2
  unsigned int *command = (unsigned int *)devAddrBase + 3;  // Change: per qualche strano motivo il test mette il comand field a base +3 e funziona...
  ACQUIRE_LOCK(&global_lock);
  *command = ACK;  // punto 3
  int deviceID = findDeviceIndex((memaddr *)devAddrBase);

  int *devSemaphore = &device_semaphores[deviceID];  // get the semaphore of the device
  (*devSemaphore)++;
  // // Change: non so se e' necessario questo controllo qua'
  // if (*devSemaphore > 1) {
  //   pcb_t *current = current_process[getPRID()];            // get the current process
  //   insertBlocked(devSemaphore, current);                   // insert the current process in the blocked list of the semaphore
  //   current_state->pc_epc += 4;                             // increment the program counter
  //   memcpy(&current->p_s, current_state, sizeof(state_t));  // save the state of the current process
  //   current_process[getPRID()] = NULL;
  //   RELEASE_LOCK(&global_lock);
  //   Scheduler();
  // } else if (*devSemaphore <= 0) {
  pcb_t *unblocked = removeBlocked(devSemaphore);
  unblocked->p_s.reg_a0 = OKCHARTRANS;  // remove the first process from the blocked list of the semaphore
  if (unblocked != NULL) {
    insertProcQ(&ready_queue, unblocked);
  }
  // }
  klog_print("-end ");
  if (current_process[getPRID()] != NULL) {
    RELEASE_LOCK(&global_lock);
    LDST(current_state);
  } else {
    RELEASE_LOCK(&global_lock);
    Scheduler();
  }
}
