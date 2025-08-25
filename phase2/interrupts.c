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

/*
gets device type (int. line) from the exception code
*/
static int getintLineNo(int);
/*
find the highest priority device causing the interrupt using the bitmap
*/
static int getdevNo(int);
/*
manages plt interrupts
*/
static void pltHandler(state_t *);
/*
manages pseudoclock interrupts
*/
static void timerHandler(state_t *);
/*
manages non-timer interrupts
*/
static void intHandler(int, state_t *);
/*
unblocks all PCBs waiting for a Pseudo-clock tick
*/
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

static void pltHandler(state_t *current_state) {
  ACQUIRE_LOCK(&global_lock);
  pcb_t *cur_proc = current_process[getPRID()];
  setTIMER(TIMESLICE);                  // Acknowledge the PLT interrupt by loading the timer with a new value using setTIMER.
  cur_proc->p_s = *current_state;       // Copy the processor state of the current CPU at the time of the exception into the Current Process’s PCB (p_s) of the current CPU
  insertProcQ(&ready_queue, cur_proc);  // Place the Current Process on the Ready Queue;
  current_process[getPRID()] = NULL;
  RELEASE_LOCK(&global_lock);
  Scheduler();
}

void timerHandler(state_t *current_state) {
  LDIT(PSECOND);       // Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
  unblockClockPCBs();  // Unblock all PCBs blocked waiting a Pseudo-clock tick.

  ACQUIRE_LOCK(&global_lock);
  pcb_PTR curr = current_process[getPRID()];
  RELEASE_LOCK(&global_lock);

  // Return control to the Current Process of the current CPU if exists
  if (curr != NULL) {
    LDST(current_state);
  }

  // If there is no Current Process to return control to, call the Scheduler
  Scheduler();
}

static void intHandler(int intlineNo, state_t *current_state) {
  // Ottieni l'indirizzo di base del dispositivo che ha interrotto
  memaddr devAddrBase = START_DEVREG + ((intlineNo - 3) * 0x80) + (getdevNo(intlineNo) * 0x10);
  unsigned int status = 0;
  pcb_t *unblocked = NULL;

  ACQUIRE_LOCK(&global_lock);

  if (intlineNo == 7) {
    /* --- Gestione Speciale e Corretta per i Terminali --- */
    termreg_t *termReg = (termreg_t *)devAddrBase;
    unsigned int trans_stat = termReg->transm_status & TERMSTATMASK;
    unsigned int recv_stat = termReg->recv_status & TERMSTATMASK;
    int *devSemaphore = NULL;

    // PRIMA controlla se l'interruzione è per la TRASMISSIONE completata
    if (trans_stat == 5) {  // Lo stato 5 significa "Character Transmitted"
      status = termReg->transm_status;
      termReg->transm_command = ACK;  // Accusa (ACK) il sub-device di trasmissione

      // Trova il semaforo corretto per il sub-device di TRASMISSIONE
      devSemaphore = &device_semaphores[findDeviceIndex((memaddr *)&termReg->transm_command)];
    }
    // ALTRIMENTI, controlla se l'interruzione è per la RICEZIONE completata
    else if (recv_stat == 5) {  // Lo stato 5 significa "Character Received"
      klog_print("Carattere ricevuto \n");
      status = termReg->recv_status;
      termReg->recv_command = ACK;  // Accusa (ACK) il sub-device di ricezione

      // Trova il semaforo corretto per il sub-device di RICEZIONE
      devSemaphore = &device_semaphores[findDeviceIndex((memaddr *)&termReg->recv_command)];
    }

    // Se abbiamo gestito un'interruzione valida (trasmissione o ricezione), sblocchiamo il processo
    if (devSemaphore != NULL) {
      (*devSemaphore)++;
      unblocked = removeBlocked(devSemaphore);
    }

  } else {
    /* --- Gestione per tutti gli altri dispositivi (Disk, Flash, Printer, etc.) --- */
    dtpreg_t *devReg = (dtpreg_t *)devAddrBase;
    status = devReg->status;
    devReg->command = ACK;

    int deviceID = findDeviceIndex((memaddr *)devAddrBase);
    int *devSemaphore = &device_semaphores[deviceID];

    (*devSemaphore)++;
    unblocked = removeBlocked(devSemaphore);
  }

  // Se un processo è stato sbloccato (da qualsiasi dispositivo), aggiorna il suo stato e mettilo in coda
  if (unblocked != NULL) {
    unblocked->p_s.reg_a0 = status;
    insertProcQ(&ready_queue, unblocked);
  }

  // Se un processo era in esecuzione, ritorna il controllo, altrimenti chiama lo scheduler
  if (current_process[getPRID()] != NULL) {
    RELEASE_LOCK(&global_lock);
    LDST(current_state);
  } else {
    RELEASE_LOCK(&global_lock);
    Scheduler();
  }
}

/*---------Helper Functions--------*/

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
