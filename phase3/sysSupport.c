#include "headers/sysSupport.h"

// Prototipi delle funzioni statiche
static void syscallSupHandler(state_t* exp_state);
static void terminateProcess(state_t* exp_state);
static void SYS3(state_t* exp_state);
static void SYS4(state_t* exp_state);
static void SYS5(state_t* exp_state);

void generalExceptionSupportHandler() {
  support_t* support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
  state_t* exp_state = &(support->sup_exceptState[GENERALEXCEPT]);

  unsigned int cause_code = (exp_state->cause & CAUSE_EXCCODE_MASK);

  if (cause_code == SYSEXCEPTION) {
    syscallSupHandler(exp_state);
  } else {
    programTrapHandler(exp_state);
  }
}

void programTrapHandler(state_t* exp_state) {
  terminateProcess(exp_state);
}

static void syscallSupHandler(state_t* exp_state) {
  int n_syscall = exp_state->reg_a0;

  switch (n_syscall) {
    case TERMINATE:
      terminateProcess(exp_state);
      break;
    case WRITEPRINTER:
      SYS3(exp_state);
      break;
    case WRITETERMINAL:
      SYS4(exp_state);
      break;
    case READTERMINAL:
      SYS5(exp_state);
      break;
    default:
      programTrapHandler(exp_state);
      break;
  }
}

static void terminateProcess(state_t* exp_state) {
  int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);
  int pid = SYSCALL(GETPROCESSID, 0, 0, 0);

  releaseAllMutex(pid);

  // 3. Segnala la terminazione e termina.
  SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
  SYSCALL(TERMPROCESS, 0, 0, 0);
}
static void SYS3(state_t* exp_state) {
  int len = exp_state->reg_a2;
  char* virtAddr = (char*)exp_state->reg_a1;
  int pid = SYSCALL(GETPROCESSID, 0, 0, 0);
  unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);


  if ((unsigned int)virtAddr < UPROCSTARTADDR || ((unsigned int)virtAddr + len) > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
    programTrapHandler(exp_state);
    return;
  }
  

  dtpreg_t* printer_device = (dtpreg_t*)DEV_REG_ADDR(IL_PRINTER, asid - 1);
  // Configura il device printer e acquisisci il mutex
  int dev_index = findDeviceIndex((memaddr*)printer_device);
  getMutex(&sharable_dev_sem[dev_index], pid);


  printer_device->data0 = (memaddr)virtAddr;
  printer_device->data1 = len;

  int status = SYSCALL(DOIO, (int)&(printer_device->command), TRANSMITCHAR, 0);
  releaseMutex(&sharable_dev_sem[dev_index], pid);

  if (status == 1) {  // READY
    exp_state->reg_a0 = len;
  } else {
    exp_state->reg_a0 = -status;
  }

  exp_state->pc_epc += 4;
  LDST(exp_state);
}

static void SYS4(state_t* exp_state) {
  int pid = SYSCALL(GETPROCESSID, 0, 0, 0);
  unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);
  char* str = (char*)exp_state->reg_a1;
  unsigned int len = exp_state->reg_a2;
    if ((unsigned int)str < UPROCSTARTADDR || ((unsigned int)str + len) > USERSTACKTOP || len <= 0 || len > MAXSTRLENG) {
      programTrapHandler(exp_state);
      return;
    }

  //It is an error to write to a terminal device from an address outside of the requesting U-proc’s logical address space ??????????
  termreg_t* term_dev = (termreg_t*)DEV_REG_ADDR(IL_TERMINAL, asid - 1);
  int ret_status = len; 

  int dev_index = findDeviceIndex((memaddr*)&term_dev->transm_command);
  getMutex(&sharable_dev_sem[dev_index], pid);
  
  for (int i = 0; i < len; i++) {
    unsigned int command = TRANSMITCHAR | (str[i] << 8);
    unsigned int retvalue = SYSCALL(DOIO, (int)&(term_dev->transm_command), command, 0);
    unsigned int termstat = retvalue & 0xFF;
    if((termstat)!=OKCHARTRANS){
      ret_status = -(int)termstat;
      break;
    }
  }
  releaseMutex(&sharable_dev_sem[dev_index], pid);

  exp_state->reg_a0 = ret_status;
  exp_state->pc_epc += 4;
  LDST(exp_state);
}


static void SYS5(state_t* exp_state) {
  char* virtAddr = (char*)exp_state->reg_a1;
  int pid = SYSCALL(GETPROCESSID, 0, 0, 0);
  unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);

  if ((unsigned int)virtAddr < UPROCSTARTADDR || (unsigned int)virtAddr >= USERSTACKTOP) {
    programTrapHandler(exp_state);
    return;
  }

  termreg_t* terminal_device = (termreg_t*)DEV_REG_ADDR(IL_TERMINAL, asid - 1);
  int i = 0; // Contatore per i caratteri letti
  
  int dev_index = findDeviceIndex((memaddr*)&terminal_device->recv_command);
  getMutex(&sharable_dev_sem[dev_index], pid);

  // Ciclo di lettura fino al newline
  while (1) {
    int status = SYSCALL(DOIO, (int)&(terminal_device->recv_command), RECEIVECHAR, 0);
    unsigned int device_status = status & 0xFF;
    unsigned char received_char = (status >> 8) & 0xFF;
    // Controlla se la lettura è andata a buon fine
    if (device_status != CHARRECV) { 
      exp_state->reg_a0 = -(int)device_status;
      break;
    }

    // Se il carattere è un newline, abbiamo finito
    if (received_char == '\n') {
      virtAddr[i] = ' ';
      i++; // Conta anche il newline come carattere letto per la strcat
      exp_state->reg_a0 = i; // Restituisci il numero di caratteri letti
      break;
    }
    
    // Altrimenti, salva il carattere nel buffer e incrementa il contatore
    virtAddr[i] = received_char;
    i++;
  }
  releaseMutex(&sharable_dev_sem[dev_index], pid);

  exp_state->pc_epc += 4;
  LDST(exp_state);
}