#include "headers/exceptions.h"

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"

extern void klog_print(char*);
extern void klog_print_dec(int);
extern void klog_print_hex(int);

/*
Function to handle syscall exceptions
*/
static void syscallHandler(state_t*);
/*
Function to handle every exception that is not a syscall nor an interrupt
*/ 
static void passUpordie(int exception, state_t*);
/*Helper function to terminate the process' progeny*/
static void terminateSubtree(pcb_t*);
static void termProc(int);

void exceptionHandler() {
  klog_print("Exception");
  unsigned int cpu_id = getPRID();                           // get the cpu id of the current process
  state_t* current_state = GET_EXCEPTION_STATE_PTR(cpu_id);  // get the current state of the process

  unsigned int cause = getCAUSE() & CAUSE_EXCCODE_MASK;  // as specified in phase 2 specs, use the bitwise AND to get the exception code

  if (CAUSE_IS_INT(getCAUSE())) {
    interruptHandler(cause, current_state);  // if the cause is an interrupt, call the interrupt handler
  } else if (cause == EXC_ECU || cause == EXC_ECM) {
    syscallHandler(current_state);
  } else if (cause >= EXC_MOD && cause <= EXC_UTLBS) {
    passUpordie(PGFAULTEXCEPT, current_state);
  } else {
    passUpordie(GENERALEXCEPT, current_state);
  }
}

static void syscallHandler(state_t* state) {
  int syscall_code = state->reg_a0;
  cpu_t end_time = 0;

  if (!(state->status & MSTATUS_MPP_MASK) && syscall_code < 0) {  // if the MPP bit is not set, the syscall was called in user mode
    state->cause = PRIVINSTR;                                     // set the cause to privileged instruction exception
    passUpordie(GENERALEXCEPT, state);                            // call the passUpordie function to handle the exception as a Program trap
    return;
  }

  if (syscall_code >= 1) {  // if the syscall code is greater than 1, it is a high level syscall
    passUpordie(GENERALEXCEPT, state);
    return;
  }

  switch (syscall_code) {
    case CREATEPROCESS:            // this call creates a new process
      ACQUIRE_LOCK(&global_lock);  // acquire the lock to avoid race conditions
      klog_print("-createproc");

      pcb_t* newPCB = allocPcb();  // allocate a new PCB

      // if the PCB is NULL, the allocation failed for lack of resources
      if (newPCB == NULL) {
        klog_print("-nomorePCB!");
        RELEASE_LOCK(&global_lock);
        state->reg_a0 = -1;  // return -1 to signal the error
        state->pc_epc += 4;
        LDST(state);
        break;
      }
      newPCB->p_s = *(state_t*)state->reg_a1;  // copy the state given in a1 register to the new PCB
      newPCB->p_supportStruct = (support_t*)state->reg_a3;
      pcb_t* parent = current_process[getPRID()];  // set the current process as the parent of the new process based on cpu number
      klog_print("-parentID:");
      klog_print_dec(parent->p_pid);
      klog_print("-procID:");
      klog_print_dec(newPCB->p_pid);
      if (parent != NULL) {
        insertChild(parent, newPCB);
      }
      insertProcQ(&ready_queue, newPCB);
      process_count++;
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      state->reg_a0 = newPCB->p_pid;  // return the pid of the new process
      state->pc_epc += 4;             // increment the program counter
      LDST(state);
      break;

    case TERMPROCESS:
      klog_print("-termproc");
      int pid = state->reg_a1;  // get the pid of the process to be terminated
      termProc(pid);
      ACQUIRE_LOCK(&global_lock);
      if (current_process[getPRID()] != NULL) {
        klog_print("-continue-end");
        RELEASE_LOCK(&global_lock);
        state->pc_epc += 4;
        LDST(state);
      } else {
        klog_print("-curr-del-end ");
        RELEASE_LOCK(&global_lock);
        Scheduler();  // call the scheduler to select the next process
      }
      break;

    case PASSEREN:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-P");
      int* semAdd1 = (int*)state->reg_a1;  // get the semaphore address

      (*semAdd1)--;        // decrement the semaphore value
      if (*semAdd1 < 0) {  // if the semaphore address is negative, its value can't be decreased
        /* Block calling process (wait for V) */
        pcb_t* current = current_process[getPRID()];  // get the current process
        insertBlocked(semAdd1, current);              // insert the current process in the blocked list of the semaphore
        state->pc_epc += 4;                           // increment the program counter
        current->p_s = *state;                        // save the state of the current process
        current_process[getPRID()] = NULL;            // set the current process to NULL
        STCK(end_time);
        current->p_time += end_time - proc_time_started[getPRID()];  // update the time of the current process
        klog_print("-blocked-end ");
        RELEASE_LOCK(&global_lock);
        Scheduler();
        break;
      } else if (*semAdd1 >= 1) {  // if the semaphore value is greater than or equal to 1, there are processes waiting on the semaphore
        /* Unblock process waiting for P */
        pcb_t* unblocked = removeBlocked(semAdd1);  // remove the first process from the blocked list of the semaphore
        if (unblocked != NULL) {
          insertProcQ(&ready_queue, unblocked);
          klog_print("-unblocked");
        }
      }

      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      state->pc_epc += 4;
      LDST(state);
      break;

    case VERHOGEN:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-V");
      int* semAdd2 = (int*)state->reg_a1;  // get the semaphore address

      (*semAdd2)++;
      if (*semAdd2 > 1) {
        /* Block calling process (wait for P) */
        pcb_t* current = current_process[getPRID()];  // get the current process
        insertBlocked(semAdd2, current);              // insert the current process in the blocked list of the semaphore
        state->pc_epc += 4;                           // increment the program counter
        current->p_s = *state;                        // save the state of the current process
        current_process[getPRID()] = NULL;
        STCK(end_time);
        current->p_time += end_time - proc_time_started[getPRID()];  // update the time of the current process
        klog_print("-blocked-end ");
        RELEASE_LOCK(&global_lock);
        Scheduler();

        return;
      } else if (*semAdd2 <= 0) {  // if the semaphore value is less than or equal to 0, there are processes waiting on the semaphore
        /* Unblock process waiting for V */
        pcb_t* unblocked = removeBlocked(semAdd2);  // remove the first process from the blocked list of the semaphore
        if (unblocked != NULL) {
          insertProcQ(&ready_queue, unblocked);
          klog_print("-unblocked");
        }
      }
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      state->pc_epc += 4;  // increment the program counter
      LDST(state);
      break;

    case DOIO:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-doio");
      memaddr* commandAddress = (memaddr*)state->reg_a1;  // get the command address
      int commandValue = state->reg_a2;                   // get the command value

      if (commandAddress == NULL) {
        RELEASE_LOCK(&global_lock);
        state->reg_a0 = -1;  // if the command address is NULL, return -1
        state->pc_epc += 4;  // increment the program counter
        LDST(state);
        break;
      }

      /* Get device semaphore */
      pcb_t* current = current_process[getPRID()];         // get the current process
      int devIndex = findDeviceIndex(commandAddress - 1);  // get the device index from the command address

      if (devIndex < 0) {
        RELEASE_LOCK(&global_lock);
        state->reg_a0 = -1;  // if the device index is not valid, return -1
        state->pc_epc += 4;  // increment the program counter
        LDST(state);
        break;
      }

      int* devSemaphore = &device_semaphores[devIndex];  // get the semaphore of the device

      /* P on device semaphore to block process */
      (*devSemaphore)--;   // decrement the semaphore value to block the process until the i/o operation is completed
      state->pc_epc += 4;  // increment the program counter
      current->p_s = *state;
      insertBlocked(devSemaphore, current);  // insert the current process in the blocked
      current_process[getPRID()] = NULL;

      STCK(end_time);
      current->p_time += end_time - proc_time_started[getPRID()];  // update the time of the current process
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);

      *commandAddress = commandValue;
      Scheduler();
      break;

    case GETTIME:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-gettime");
      pcb_t* proc = current_process[getPRID()];  // get the current process

      // Update accumolated time and reset time started
      STCK(end_time);
      proc->p_time += end_time - proc_time_started[getPRID()];
      proc_time_started[getPRID()] = end_time;

      state->reg_a0 = proc->p_time;  // return the time of the current process
      state->pc_epc += 4;            // increment the program counter
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      LDST(state);
      break;

    case CLOCKWAIT:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-clockwait");
      pcb_t* cur = current_process[getPRID()];

      if (insertBlocked(&device_semaphores[PSEUDOCLOCK], cur) == TRUE) {
        RELEASE_LOCK(&global_lock);
        PANIC();  // error: no free semaphores
      }

      state->pc_epc += 4;  // increment the program counter
      cur->p_s = *state;   // save the state of the current process
      current_process[getPRID()] = NULL;
      STCK(end_time);
      cur->p_time += end_time - proc_time_started[getPRID()];  // update the time of the current process
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      Scheduler();  // pass the control to the scheduler
      break;

    case GETSUPPORTPTR:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-getsupptr");
      pcb_t* current1 = current_process[getPRID()];        // get the current process
      state->reg_a0 = (memaddr)current1->p_supportStruct;  // return the support struct of the current process
      state->pc_epc += 4;                                  // increment the program counter
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      LDST(state);  // load the state of the current process
      break;

    case GETPROCESSID:
      ACQUIRE_LOCK(&global_lock);
      klog_print("-getpid");
      pcb_t* current3 = current_process[getPRID()];  // get the current process
      int isGetParent = state->reg_a1;

      if (isGetParent == 0) {
        // return the pid of the current process
        state->reg_a0 = current3->p_pid;
      } else {
        // return the pid of the current process's parent, or 0 if NULL
        state->reg_a0 = current3->p_parent == NULL ? 0 : current3->p_parent->p_pid;
      }

      state->pc_epc += 4;
      klog_print("-end ");
      RELEASE_LOCK(&global_lock);
      LDST(state);  // load the state of the current process
      break;

    default:
      klog_print("!Nonexistent Syscall Nucleus service!");
      PANIC();
      break;
  }
}

static void passUpordie(int exception, state_t* exc_state) {
  ACQUIRE_LOCK(&global_lock);
  int cpu_id = getPRID();
  pcb_t* current = current_process[cpu_id];

  if (current == NULL) {
    klog_print("-nocurrproc-end");
    RELEASE_LOCK(&global_lock);
    Scheduler();
  }

  // Se il processo non ha supportStruct → termina
  if (current->p_supportStruct == NULL) {
    klog_print("-nosuppstruct-end");
    RELEASE_LOCK(&global_lock);
    termProc(0);  // termina processo corrente
    Scheduler();
  }

  // PASS UP: copia lo stato dell'eccezione
  support_t* sup = current->p_supportStruct;
  sup->sup_exceptState[exception] = *exc_state;
  context_t* ctx = &sup->sup_exceptContext[exception];

  klog_print("-pud-end ");
  RELEASE_LOCK(&global_lock);

  LDCXT(ctx->stackPtr, ctx->status, ctx->pc);  // load the context
}

/*--------Helper Functions--------*/

static void termProc(int pid) {
  ACQUIRE_LOCK(&global_lock);

  /* Find the pcb with the given id */
  pcb_t* tbt = NULL;  // initialize the pointer to the process to be terminated
  if (pid == 0) {
    tbt = current_process[getPRID()];  // if the pid is 0, terminate the current process
  } else {
    struct pcb_t* iter;
    /* Search in ready_queue */
    list_for_each_entry(iter, &ready_queue, p_list) {
      if (iter->p_pid == pid) {
        tbt = iter;
        break;
      }
    }
    if (tbt == NULL) {
      /* Search processes blocked on semaphores */
      tbt = getProc(pid);
      if (tbt == NULL) {
        /* If the process wasn't found */
        klog_print("!running on another cpu!");
        RELEASE_LOCK(&global_lock);
        return;
      };
    }
  }

  /* Remove and free pcb's subtree */
  terminateSubtree(tbt);       // terminate the subtree of the process to be terminated
  RELEASE_LOCK(&global_lock);  // release the lock
}


static void terminateSubtree(pcb_t* process) {
  // Terminate all children
  while (!emptyChild(process)) {
    pcb_t* child = removeChild(process);  // remove the first child of the process
    terminateSubtree(child);
  }
  // Terminate calling process
  if (process->p_semAdd != NULL) {
    // Process blocked on semaphore
    outBlocked(process);
    (*process->p_semAdd)++;
  } else if (outProcQ(&ready_queue, process) == NULL) {  // Process on ready queue
    // Process running (or doesn't exist?)
    klog_print("!process is running on another cpu!");
    return;
  };
  process_count--;
  if (process->p_parent != NULL) {
    outChild(process);  // remove the process from the parent's children list
  }
  klog_print("-term pid:");
  klog_print_dec(process->p_pid);
  if (process == current_process[getPRID()]) current_process[getPRID()] = NULL;  // set the current process to NULL
  freePcb(process);
}

// Given the address of the device register, returns a unique index for each device
// that identifies the corresponding semaphore.
// Differentiates sub-devices in the case of terminal registers.
int findDeviceIndex(memaddr* devRegAddress) {
  unsigned int offset = (unsigned int)(devRegAddress)-START_DEVREG;
  int i = -1;
  if (offset >= 32 * 0x10) {
    i = 32 + ((offset - (32 * 0x10)) / 0x8);
  } else {
    i = offset / 0x10;  
  }
  return i; 
}
