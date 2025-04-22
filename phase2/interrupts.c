#include "headers/interrupts.h"

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "headers/initial.h"

#define TERMSTATMASK 0xFF
#define RECVD 5

extern void klog_print(char *);
extern void klog_print_dec(int);
extern void klog_print_hex(int);

static int getintLineNo(int);
static int getdevNo(int);
static void pltHandler(state_t *);
static void timerHandler(state_t *);
static void intHandler(int, state_t *);
static void unblockClockPCBs();

void interruptHandler(unsigned int int_code, state_t *current_state) {
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

// manages plt interrupts
static void pltHandler(state_t *current_state) {
  ACQUIRE_LOCK(&global_lock);
  klog_print("-PLT");
  pcb_t *cur_proc = current_process[getPRID()];
  setTIMER(TIMESLICE);                  // Acknowledge the PLT interrupt by loading the timer with a new value using setTIMER.
  cur_proc->p_s = *current_state;       // Copy the processor state of the current CPU at the time of the exception into the Current Process’s PCB (p_s) of the current CPU
  insertProcQ(&ready_queue, cur_proc);  // Place the Current Process on the Ready Queue;
  current_process[getPRID()] = NULL;
  klog_print("-end ");
  RELEASE_LOCK(&global_lock);
  Scheduler();
}

// manages pseudoclock interrupts
void timerHandler(state_t *current_state) {
  klog_print("-timer");
  LDIT(PSECOND);       // Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
  unblockClockPCBs();  // Unblock all PCBs blocked waiting a Pseudo-clock tick.

  ACQUIRE_LOCK(&global_lock);
  pcb_PTR curr = current_process[getPRID()];
  RELEASE_LOCK(&global_lock);

  // Return control to the Current Process of the current CPU if exists
  if (curr != NULL) {
    klog_print("-loadingCurr ");
    LDST(current_state);
  }

  // If there is no Current Process to return control to, call the Scheduler
  klog_print("-end ");
  Scheduler();
}

void intHandler(int intlineNo, state_t *current_state) {
  klog_print("-int");
  int devNo = getdevNo(intlineNo);
  memaddr devAddrBase = START_DEVREG + ((intlineNo - 3) * 0x80) + (devNo * 0x10);  // punto 1
  unsigned int status = 0;

  ACQUIRE_LOCK(&global_lock);
  if (intlineNo == 7) {
    /* Terminal Interrupt */
    termreg_t *termReg = (termreg_t *)devAddrBase;
    unsigned int trans_stat = termReg->transm_status & TERMSTATMASK;
    if (2 <= trans_stat && trans_stat <= OKCHARTRANS) {  // If the transmit status code is an error or Char Received,
                                                         // then the sub-device has an interrupt pending
      status = termReg->transm_status;
      termReg->transm_command = ACK;
      devAddrBase += 0x8;
    } else {  // Int is recv
      status = termReg->recv_status;
      termReg->recv_command = ACK;
    }
  } else {
    /* Generic device Interrupt */
    dtpreg_t *devReg = (dtpreg_t *)devAddrBase;
    status = devReg->status;  // punto 2
    devReg->command = ACK;
  }

  /* V the device semaphore */
  int deviceID = findDeviceIndex((memaddr *)devAddrBase);
  int *devSemaphore = &device_semaphores[deviceID];  // get the semaphore of the device
  (*devSemaphore)++;
  pcb_t *unblocked = removeBlocked(devSemaphore);  // remove the first process from the blocked list of the semaphore
  if (unblocked != NULL) {
    unblocked->p_s.reg_a0 = status;
    insertProcQ(&ready_queue, unblocked);
  }
  klog_print("-end ");
  if (current_process[getPRID()] != NULL) {
    RELEASE_LOCK(&global_lock);
    LDST(current_state);
  } else {
    RELEASE_LOCK(&global_lock);
    Scheduler();
  }
}

/*---------Helper Functions--------*/

// gets device type (int. line) from the exception code
static int getintLineNo(int int_code) {
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

// find the highest priority device causing the interrupt using the bitmap
int getdevNo(int intlineNo) {
  unsigned int bitmapValue = *((unsigned int *)(INTERRUPT_BITMAP_ADDRESS + (intlineNo - 3) * 4));  // go to the corret oword of the bitmap
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

static void unblockClockPCBs() {
  pcb_t *pcb = NULL;
  ACQUIRE_LOCK(&global_lock);
  while ((pcb = removeBlocked(&device_semaphores[PSEUDOCLOCK])) != NULL) {
    insertProcQ(&ready_queue, pcb);
  }
  RELEASE_LOCK(&global_lock);
}
