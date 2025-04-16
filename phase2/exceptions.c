#include "headers/exceptions.h"

#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"

extern void klog_print(char*);
extern void klog_print_dec(int);
extern void klog_print_hex(int);

void passUpordie(int exception);

/*void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST(GET_EXCEPTION_STATE_PTR(prid));
}*/

void exceptionHandler() {
  cpu_t cpu_time_init, cpu_time_end;
  unsigned int cpu_id = getPRID();                             // get the cpu id of the current process
  state_t* current_state = (GET_EXCEPTION_STATE_PTR(cpu_id));  // get the current state of the process

  STCK(cpu_time_init);  // call the timer function to update the time of the current process

  unsigned int cause = getCAUSE() & CAUSE_EXCCODE_MASK;  // as specified in phase 2 specs, use the bitwise AND to get the exception code

  while (CAUSE_IS_INT(getCAUSE())) {  // cycle to reenter the handler immediately if there are more than one interrupt pending
    interruptHandler(current_state);  // if the cause is an interrupt, call the interrupt handler
  }
  if (cause == EXC_ECU || cause == EXC_ECM) {
    syscallHandler(current_state);
  } else if (cause >= EXC_MOD && cause <= EXC_UTLBS) {
    tlbExceptionHandler(current_state);
  } else {
    programTrapHandler(current_state);
  }

  // Change: ste robe qua in teoria non vengono mai eseguite, dato che o si chiama Scheduler, o si fa LDST o passUpOrDie.
  // Dobbiamo capire in quali casi effettivamente va aumentato il tempo accumolato dal processo.
  pcb_t* curr = current_process[cpu_id];  // get the current process

  STCK(cpu_time_end);  // call the timer function to update the time of the current process

  curr->p_time += (cpu_time_end - cpu_time_init);  // update the time of the current process

  // SE LE 3 RIGHE QUA SOPRA DANNO PROBLEMI RIVEDERE IMPLEMENTAZIONE
}

// this helper function is used to terminate the current process subtree
static void terminateSubtree(pcb_t* process) {
  while (!emptyChild(process)) {
    pcb_t* child = removeChild(process);  // remove the first child of the process
    if (child != NULL) {
      terminateSubtree(child);
      outProcQ(&ready_queue, child);
      process_count--;
      freePcb(child);
    }
  }
}

// Passato l'indirizzo del device register, restituisce un indice unico per ogni device
// che identifica il semaforo corrispondente.
// Distingue i sub-device nel caso dei register dei terminali.
int findDeviceIndex(memaddr* devRegAddress) {
  unsigned int offset = (unsigned int)(devRegAddress)-START_DEVREG;
  int i = -1;
  if (offset >= 32 * 0x10) {
    i = 32 + ((offset - (32 * 0x10)) / 0x8);  // renzo davoli
  } else {
    i = offset / 0x10;  // renzo davoli
  }
  return i;  // renzo davoli
}

static void syscallHandler(state_t* state) {
  klog_print("inizio gestione syscall");
  int syscall_code = state->reg_a0;

  if (!(state->status & MSTATUS_MPP_MASK) && syscall_code < 0) {  // if the MPP bit is not set, the syscall was called in user mode
    state->cause = PRIVINSTR;                                     // set the cause to privileged instruction exception
    passUpordie(GENERALEXCEPT);                                   // call the passUpordie function to handle the exception as a Program trap
    return;
  }

  if (syscall_code >= 1) {  // if the syscall code is greater than 1, it is a syscall

    klog_print("maggiore di 1");
    passUpordie(GENERALEXCEPT);
    return;
  }

  klog_print("inizio switch");
  switch (syscall_code) {
    case CREATEPROCESS:  // this call creates a new process
      klog_print("createprocess start");
      ACQUIRE_LOCK(&global_lock);  // acquire the lock to avoid race conditions
      pcb_t* newPCB = allocPcb();  // allocate a new PCB
      if (newPCB == NULL) {        // if the PCB is NULL, the allocation failed for lack of resources
        RELEASE_LOCK(&global_lock);
        state->reg_a0 = -1;  // return -1 to signal the error
        break;
      }

      memcpy(&newPCB->p_s, (state_t*)state->reg_a1, sizeof(state_t));  // copy the state given in a1 register to the new PCB
      newPCB->p_supportStruct = (support_t*)(state->reg_a3);           // set the support struct to the one given in a3 register

      pcb_t* parent = current_process[getPRID()];  // set the current process as the parent of the new process based on cpu number
      if (parent != NULL) {
        insertChild(parent, newPCB);
      }

      insertProcQ(&ready_queue, newPCB);
      process_count++;
      RELEASE_LOCK(&global_lock);
      state->reg_a0 = newPCB->p_pid;  // return the pid of the new process
      break;

    case TERMPROCESS:
      klog_print("termprocess start");
      ACQUIRE_LOCK(&global_lock);  // this call terminates the current process
      int pid = state->reg_a1;     // get the pid of the process to be terminated
      pcb_t* tbt = NULL;           // initialize the pointer to the process to be terminated

      if (pid == 0) {
        tbt = current_process[getPRID()];  // if the pid is 0, terminate the current process
      } else {
        struct list_head* iter;
        list_for_each(iter, &ready_queue) {
          pcb_t* pcb = container_of(iter, pcb_t, p_list);
          if (pcb->p_pid == pid) {
            tbt = pcb;
            break;
          }
        }
      }
      RELEASE_LOCK(&global_lock);
      if (tbt == NULL) {
        state->reg_a0 = -1;  // if the process to be terminated is not found, return -1
        break;
      }

      if (!emptyChild(tbt)) {
        terminateSubtree(tbt);  // terminate the subtree of the process to be terminated
      }

      if (tbt->p_parent != NULL) {
        outChild(tbt);  // remove the process from the parent's children list
      }

      outProcQ(&ready_queue, tbt);
      process_count--;
      freePcb(tbt);                       // free the PCB
      current_process[getPRID()] = NULL;  // set the current process to NULL
      Scheduler();                        // call the scheduler to select the next process
      break;

    case PASSEREN:
      klog_print("passeren start");
      ACQUIRE_LOCK(&global_lock);
      int* semAdd1 = (int*)state->reg_a1;  // get the semaphore address
      (*semAdd1)--;                        // decrement the semaphore value
      if (*semAdd1 < 0) {                  // if the semaphore address is negative, its value can't be decreased
        /* Block calling process (wait for V) */
        pcb_t* current = current_process[getPRID()];  // get the current process
        insertBlocked(semAdd1, current);              // insert the current process in the blocked list of the semaphore

        state->pc_epc += 4;                               // increment the program counter
        memcpy(&(current->p_s), state, sizeof(state_t));  // save the state of the current process

        RELEASE_LOCK(&global_lock);
        klog_print("passeren scheduler");
        Scheduler();
        klog_print("passeren scheduler end");
        break;
      }

      // Change: anche la P puo' sbloccare roba in teoria
      else if (*semAdd1 >= 1) {  // if the semaphore value is greater than or equal to 1, there are processes waiting on the semaphore
        /* Unblock process waiting for P */
        pcb_t* unblocked = removeBlocked(semAdd1);  // remove the first process from the blocked list of the semaphore
        if (unblocked != NULL) {
          insertProcQ(&ready_queue, unblocked);
        }
      }

      RELEASE_LOCK(&global_lock);
      state->pc_epc += 4;
      LDST(state);
      break;

    case VERHOGEN:
      klog_print("verhogen");
      ACQUIRE_LOCK(&global_lock);
      int* semAdd2 = (int*)state->reg_a1;  // get the semaphore address

      (*semAdd2)++;
      klog_print("verhogen 2");
      if (*semAdd2 > 1) {
        /* Block calling process (wait for P) */
        klog_print("verhogen 3");
        pcb_t* current = current_process[getPRID()];      // get the current process
        insertBlocked(semAdd2, current);                  // insert the current process in the blocked list of the semaphore
        state->pc_epc += 4;                               // increment the program counter
        memcpy(&(current->p_s), state, sizeof(state_t));  // save the state of the current process
        current_process[getPRID()] = NULL;
        RELEASE_LOCK(&global_lock);
        Scheduler();

        return;
      } else if (*semAdd2 <= 0) {  // if the semaphore value is less than or equal to 0, there are processes waiting on the semaphore
        /* Unblock process waiting for V */
        pcb_t* unblocked = removeBlocked(semAdd2);  // remove the first process from the blocked list of the semaphore
        if (unblocked != NULL) {
          insertProcQ(&ready_queue, unblocked);
        }
      }
      RELEASE_LOCK(&global_lock);
      state->pc_epc += 4;  // increment the program counter
      LDST(state);
      break;

    case DOIO:
      klog_print("doio start");
      ACQUIRE_LOCK(&global_lock);
      memaddr* commandAddress = (memaddr*)state->reg_a1;  // get the command address
      int commandValue = state->reg_a2;                   // get the command value

      if (commandAddress == NULL) {
        state->reg_a0 = -1;  // if the command address is NULL, return -1
        RELEASE_LOCK(&global_lock);
        // Change: dato che non lo facciamo piu' alla fine, dobbiamo caricare il processo qui'
        state->pc_epc += 4;  // increment the program counter
        LDST(state);
        break;
      }

      /* Get device semaphore */
      pcb_t* current = current_process[getPRID()];         // get the current process
      int devIndex = findDeviceIndex(commandAddress - 3);  // get the device index from the command address
      klog_print("doio index:");
      klog_print_dec(devIndex);

      if (devIndex < 0) {
        state->reg_a0 = -1;  // if the device index is not valid, return -1
        RELEASE_LOCK(&global_lock);
        // Change: dato che non lo facciamo piu' alla fine, dobbiamo caricare il processo qui'
        state->pc_epc += 4;  // increment the program counter
        LDST(state);
        break;
      }
      int* devSemaphore = &device_semaphores[devIndex];  // get the semaphore of the device

      /* P on device semaphore to block process */
      (*devSemaphore)--;                        // decrement the semaphore value to block the process until the i/o operation is completed
      state->pc_epc += 4;                       // increment the program counter
      state->reg_a0 = *(commandAddress - 0x4);  // return the value of the status field in device register

      // Change: per qualche motivo quando current si risveglia sembra ripartire dall'inizio di test, ci guardo domani
      memcpy(&(current->p_s), state, sizeof(state_t));
      insertBlocked(devSemaphore, current);  // insert the current process in the blocked
      current_process[getPRID()] = NULL;

      RELEASE_LOCK(&global_lock);
      klog_print("4 \n");
      *commandAddress = commandValue;
      Scheduler();
      break;

    case GETTIME:
      klog_print("gettime start");
      ACQUIRE_LOCK(&global_lock);
      pcb_t* proc = current_process[getPRID()];  // get the current process
      state->reg_a0 = proc->p_time;              // return the time of the current process
      RELEASE_LOCK(&global_lock);
      break;

    case CLOCKWAIT:
      ACQUIRE_LOCK(&global_lock);
      pcb_t* cur = current_process[getPRID()];  // get the current process

      if (insertBlocked(&device_semaphores[PSEUDOCLOCK], cur) == TRUE) {
        PANIC();  // errore: non c'è memoria per inserirlo nella ASL
      }

      current_process[getPRID()] = NULL;
      Scheduler();  // pass the control to the scheduler
      break;

    case GETSUPPORTPTR:
      klog_print("getsupportptr start");
      ACQUIRE_LOCK(&global_lock);
      pcb_t* current1 = current_process[getPRID()];        // get the current process
      state->reg_a0 = (memaddr)current1->p_supportStruct;  // return the support struct of the current process
      RELEASE_LOCK(&global_lock);
      break;

    case GETPROCESSID:
      ACQUIRE_LOCK(&global_lock);
      pcb_t* current3 = current_process[getPRID()];  // get the current process
      state->reg_a0 = current3->p_pid;               // return the pid of the current process
      RELEASE_LOCK(&global_lock);
      break;
  }
}

// -----------------------------------------------------------------------------------------------------------
// la roba qua sotto devo controllarla
// -----------------------------------------------------------------------------------------------------------

void tlbExceptionHandler(state_t* state) {
  int cause = state->cause & CAUSE_EXCCODE_MASK;  // get the cause of the exception
  if (cause == 0) {                               // if the cause is a TLB refill
    uTLB_RefillHandler();
  } else {
    passUpordie(GENERALEXCEPT);
  }
}

void programTrapHandler(state_t* state) {
  int cause = state->cause & CAUSE_EXCCODE_MASK;  // get the cause of the exception
  if (cause == 0) {                               // if the cause is a TLB refill
    uTLB_RefillHandler();
  } else {
    passUpordie(GENERALEXCEPT);
  }
}

void passUpordie(int exception) {
  ACQUIRE_LOCK(&global_lock);

  int cpu_id = getPRID();
  state_t* exc_state = GET_EXCEPTION_STATE_PTR(cpu_id);

  pcb_t* current = current_process[cpu_id];

  if (current == NULL) {
    RELEASE_LOCK(&global_lock);
    PANIC();  // kernel panic: eccezione senza processo
  }

  // Se il processo non ha supportStruct → termina
  if (current->p_supportStruct == NULL) {
    RELEASE_LOCK(&global_lock);
    SYSCALL(TERMPROCESS, 0, 0, 0);  // termina processo
    PANIC();                        // non dovrebbe tornare
  }

  // PASS UP: copia lo stato dell'eccezione
  support_t* sup = current->p_supportStruct;
  sup->sup_exceptState[exception] = *exc_state;

  context_t* ctx = &sup->sup_exceptContext[exception];

  RELEASE_LOCK(&global_lock);

  state_t new_state;
  new_state.status = ctx->status;
  new_state.pc_epc = ctx->pc;
  new_state.gpr[29] = ctx->stackPtr;  // $sp = gpr[29]

  LDST(&new_state);
}
