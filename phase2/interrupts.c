#include "headers/interrupts.h"
#define SAVE_STATE(x) (current_process[x]->p_s = *GET_EXCEPTION_STATE_PTR(x))

extern void klog_print(char *);
extern void klog_print_dec(int);

void interruptHandler(state_t *current_state) {
  klog_print("--Inizio gestione Interrupt--");
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
  setTIMER(TIMESLICE);  // Acknowledge the PLT interrupt by loading the timer with a new value using setTIMER.
  memcpy(&current_process[getPRID()]->p_s, current_state, sizeof(state_t));
  // Copy the processor state of the current CPU at the time of the exception into the Current Process’s PCB (p_s) of the current CPU
  ACQUIRE_LOCK(&global_lock);
  insertProcQ(&ready_queue, current_process[getPRID()]);  // Place the Current Process on the Ready Queue;
  RELEASE_LOCK(&global_lock);
  Scheduler();
}

void UNBLOCKALLWAITINGCLOCKPCBS() {
  pcb_t *pcb = removeBlocked(&device_semaphores[PSEUDOCLOCK]);
  if (pcb != NULL) {
    ACQUIRE_LOCK(&global_lock);
    insertProcQ(&ready_queue, pcb);
    RELEASE_LOCK(&global_lock);
  }
}

// gestisce gli interrupt da pseudoclock
void timerHandler(state_t *current_state) {
  klog_print("--Timer Handler--");
  LDIT(PSECOND);                 // Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
  UNBLOCKALLWAITINGCLOCKPCBS();  // Unblock all PCBs blocked waiting a Pseudo-clock tick.
  pcb_PTR curr = current_process[getPRID()];
  if (curr != NULL) {
    LDST(current_state);  // Return control to the Current Process of the current CPU if exists: perform a LDST on the saved exception state of the current CPU.
  }
  // it is also possible that there is no Current Process to return control to. This will be the case when the Scheduler executes the WAIT instruction instead of dispatching a process for execution
  Scheduler();
}
void intHandler(int intlineNo, state_t *current_state) {
  klog_print("--Int Handler--");
  int devNo = getdevNo(intlineNo);
  memaddr devAddrBase = START_DEVREG + ((intlineNo - 3) * 0x80) + (devNo * 0x10);  // punto 1
  // come sacrosanta minchia si fa a capire quale dei due subdevice del terminal (con lo schema alla fine del pdf, in device registers) causa l'interrupt????
  if (intlineNo == 7) {
    unsigned int *writestatusaddress = (unsigned int *)devAddrBase + 0x8;
    int TERMINALWRITEINTERRUPT = *writestatusaddress;  // si spera che questo funzioni
    if (TERMINALWRITEINTERRUPT != 0) {
      klog_print("interrupt is read");
      devAddrBase += 0x8;
    }
  }
  klog_print_dec(intlineNo);

  unsigned int status = *(unsigned int *)devAddrBase;       // punto 2
  unsigned int *command = (unsigned int *)devAddrBase + 3;  // Change: per qualche strano motivo il test mette il comand field a base +3 e funziona...
  ACQUIRE_LOCK(&global_lock);
  *command = ACK;  // punto 3
  int deviceID = findDeviceIndex((memaddr *)devAddrBase);
  klog_print_dec(deviceID);

  int *devSemaphore = &device_semaphores[deviceID];  // get the semaphore of the device
  (*devSemaphore)++;
  klog_print("verhogen 2");
  // Change: non so se e' necessario questo controllo qua'
  if (*devSemaphore > 1) {
    klog_print("verhogen 3");
    pcb_t *current = current_process[getPRID()];            // get the current process
    insertBlocked(devSemaphore, current);                   // insert the current process in the blocked list of the semaphore
    current_state->pc_epc += 4;                             // increment the program counter
    memcpy(&current->p_s, current_state, sizeof(state_t));  // save the state of the current process
    current_process[getPRID()] = NULL;
    RELEASE_LOCK(&global_lock);
    Scheduler();
  } else if (*devSemaphore <= 0) {
    klog_print("sem < 0");  // if the semaphore value is less than or equal to 0, there are processes waiting on the semaphore
    pcb_t *unblocked = removeBlocked(devSemaphore);
    unblocked->p_s.reg_a0 = status;  // remove the first process from the blocked list of the semaphore
    if (unblocked != NULL) {
      insertProcQ(&ready_queue, unblocked);
    }
  }
  RELEASE_LOCK(&global_lock);
  if (current_process[getPRID()] != NULL) {
    klog_print("--CP not null, loading state--");
    LDST(current_state);
  } else {
    klog_print("--CP null, calling Scheduly--");
    Scheduler();
  }
}
