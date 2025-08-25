#include "headers/sysSupport.h"

extern void klog_print(char*);
extern void klog_print_dec(int);
extern void klog_print_hex(unsigned int);

// Prototipi delle funzioni statiche
static void syscallSupHandler(state_t* exp_state);
static void terminateProcess(state_t* exp_state);
static void SYS3(state_t* exp_state);
static void SYS4(state_t* exp_state);
static void SYS5(state_t* exp_state);

void generalExceptionSupportHandler() {
  klog_print("sysSupport: --- Entrato in generalExceptionSupportHandler ---\n");
  support_t* support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
  state_t* exp_state = &(support->sup_exceptState[GENERALEXCEPT]);

  // --- BLOCCO DI DEBUG FORENSE ---
  klog_print("  FORENSE: --- Stato della Page Table al momento del crash ---\n");
  unsigned int fault_vpn = (exp_state->entry_hi >> VPNSHIFT);
  int p_index;
  if (fault_vpn == (USERSTACKTOP >> VPNSHIFT) - 1) {
    p_index = MAXPAGES - 1;
  } else {
    p_index = fault_vpn - (UPROCSTARTADDR >> VPNSHIFT);
  }

  if (p_index >= 0 && p_index < MAXPAGES) {
    pteEntry_t* pte_al_crash = &support->sup_privatePgTbl[p_index];
    klog_print("  FORENSE: Ispezione della PTE per l'indice: ");
    klog_print_dec(p_index);
    klog_print("\n");
    klog_print("  FORENSE: Valore di pte_entryLO trovato: ");
    klog_print_hex(pte_al_crash->pte_entryLO);
    klog_print("\n");
  }
  klog_print("  FORENSE: --- Fine Blocco ---\n");
  // --- FINE BLOCCO DI DEBUG FORENSE ---
  unsigned int cause_code = (exp_state->cause & CAUSE_EXCCODE_MASK);

  klog_print("sysSupport: ASID del processo: ");
  klog_print_dec(support->sup_asid);
  klog_print(", Causa eccezione: ");
  klog_print_dec(cause_code);
  klog_print("\n");

  if (cause_code == SYSEXCEPTION) {
    klog_print("sysSupport: Eccezione di tipo SYSCALL. Smistamento...\n");
    syscallSupHandler(exp_state);
  } else {
    klog_print("sysSupport: Eccezione di tipo Program Trap. Avvio terminazione...\n");
    programTrapHandler(exp_state);
  }
}

void programTrapHandler(state_t* exp_state) {
  klog_print("sysSupport: --- Esecuzione di programTrapHandler ---\n");
  terminateProcess(exp_state);
}

static void syscallSupHandler(state_t* exp_state) {
  int n_syscall = exp_state->reg_a0;
  klog_print("sysSupport: Gestione SYSCALL numero: ");
  klog_print_dec(n_syscall);
  klog_print("\n");

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
      klog_print("sysSupport: SYSCALL non valida! Trattata come Program Trap.\n");
      programTrapHandler(exp_state);
      break;
  }
}

static void terminateProcess(state_t* exp_state) {
  int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);
  int pid = SYSCALL(GETPROCESSID, 0, 0, 0);

  klog_print("sysSupport: --- Inizio terminazione per ASID: ");
  klog_print_dec(asid);
  klog_print(", PID: ");
  klog_print_dec(pid);
  klog_print(" ---\n");

  releaseAllMutex(pid);

  // 3. Segnala la terminazione e termina.
  SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
  SYSCALL(TERMPROCESS, 0, 0, 0);
}
static void SYS3(state_t* exp_state) {
  int len = exp_state->reg_a2;
  char* virtAddr = (char*)exp_state->reg_a1;
  unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);

  klog_print("sysSupport: Inizio SYS3 (WRITEPRINTER) per ASID: ");
  klog_print_dec(asid);
  klog_print("\n  Indirizzo virtuale: ");
  klog_print_hex((unsigned int)virtAddr);
  klog_print(", Lunghezza: ");
  klog_print_dec(len);
  klog_print("\n");

  if ((unsigned int)virtAddr < UPROCSTARTADDR || ((unsigned int)virtAddr + len) > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
    klog_print("sysSupport: SYS3 ERRORE - Parametri non validi. Terminazione...\n");
    programTrapHandler(exp_state);
    return;
  }

  dtpreg_t* printer_device = (dtpreg_t*)DEV_REG_ADDR(IL_PRINTER, asid - 1);
  printer_device->data0 = (memaddr)virtAddr;
  printer_device->data1 = len;

  int status = SYSCALL(DOIO, (int)&(printer_device->command), TRANSMITCHAR, 0);

  klog_print("sysSupport: SYS3 DOIO completato con stato: ");
  klog_print_dec(status);
  klog_print("\n");

  if (status == 1) {  // READY
    exp_state->reg_a0 = len;
  } else {
    exp_state->reg_a0 = -status;
  }

  exp_state->pc_epc += 4;
  LDST(exp_state);
}

static void SYS4(state_t* exp_state) {
  // unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);
  //
  // // Controlliamo se è la prima volta che entriamo per questa SYSCALL
  // // Se a0 è ancora WRITETERMINAL, è il primo ingresso.
  // if (exp_state->reg_a0 == WRITETERMINAL) {
  //   // Inizializziamo lo stato della nostra operazione usando i registri temporanei
  //   exp_state->reg_t0 = 0;                  // t0 = contatore dei caratteri trasmessi (inizia da 0)
  //   exp_state->reg_t1 = exp_state->reg_a1;  // t1 = indirizzo della stringa
  //   exp_state->reg_t2 = exp_state->reg_a2;  // t2 = lunghezza della stringa
  // }
  //
  // // Recuperiamo lo stato (contatore, indirizzo, lunghezza) dai registri salvati
  // int chars_transmitted = exp_state->reg_t0;
  // char* virtAddr = (char*)exp_state->reg_t1;
  // int len = exp_state->reg_t2;
  //
  // // Controlliamo se abbiamo finito di trasmettere
  // if (chars_transmitted >= len) {
  //   // OPERAZIONE COMPLETATA
  //   exp_state->reg_t0 = 0;                  // Pulisci il contatore per la prossima SYSCALL
  //   exp_state->reg_a0 = chars_transmitted;  // Imposta il valore di ritorno
  //   exp_state->pc_epc += 4;                 // Avanza il PC OLTRE la SYSCALL originale
  //   LDST(exp_state);                        // Ritorna al processo utente
  //   return;
  // }
  //
  // // Se non abbiamo finito, prepariamo e inviamo il prossimo carattere
  // termreg_t* term_dev = (termreg_t*)DEV_REG_ADDR(IL_TERMINAL, asid - 1);
  // unsigned int char_to_send = virtAddr[chars_transmitted];
  // unsigned int command = TRANSMITCHAR | (char_to_send << 8);
  //
  // // Incrementiamo il nostro contatore PRIMA di bloccare il processo.
  // // Questo stato aggiornato verrà salvato dal Nucleus.
  // exp_state->reg_t0 = chars_transmitted + 1;
  //
  // // Esegui la DOIO bloccante. Il processo si fermerà qui.
  // // Al suo risveglio, il PC non sarà avanzato, e rieseguirà la SYSCALL,
  // // facendoci rientrare in questa funzione con il contatore (t0) aggiornato.
  // SYSCALL(DOIO, (int)&(term_dev->transm_command), command, 0);

  unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);
  char* str = (char*)exp_state->reg_a1;
  unsigned int len = exp_state->reg_a2;
  termreg_t* term_dev = (termreg_t*)DEV_REG_ADDR(IL_TERMINAL, asid - 1);

  for (int i = 0; i < len; i++) {
    unsigned int command = TRANSMITCHAR | (str[i] << 8);
    SYSCALL(DOIO, (int)&(term_dev->transm_command), command, 0);
  }
  exp_state->reg_a0 = len;
  exp_state->pc_epc += 4;
  LDST(exp_state);
}

static void SYS5(state_t* exp_state) {
  char* virtAddr = (char*)exp_state->reg_a1;
  unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);

  klog_print("sysSupport: Inizio SYS5 (READTERMINAL) per ASID: ");
  klog_print_dec(asid);
  klog_print("\n  Indirizzo buffer virtuale: ");
  klog_print_hex((unsigned int)virtAddr);
  klog_print("\n");

  if ((unsigned int)virtAddr < UPROCSTARTADDR || (unsigned int)virtAddr >= USERSTACKTOP) {
    klog_print("sysSupport: SYS5 ERRORE - Indirizzo non valido. Terminazione...\n");
    programTrapHandler(exp_state);
    return;
  }

  termreg_t* terminal_device = (termreg_t*)DEV_REG_ADDR(IL_TERMINAL, asid - 1);
  int status = SYSCALL(DOIO, (int)&(terminal_device->recv_command), RECEIVECHAR, 0);

  unsigned int device_status = status & 0xFF;
  unsigned int chars_read = status >> 8;

  klog_print("sysSupport: SYS5 DOIO completato con stato: ");
  klog_print_dec(device_status);
  klog_print(", Caratteri letti: ");
  klog_print_dec(chars_read);
  klog_print("\n");

  if (device_status == 5) {  // CHAR_RECEIVED
    exp_state->reg_a0 = 1;
    virtAddr[0] = chars_read;
  } else {
    exp_state->reg_a0 = -(int)device_status;
  }

  exp_state->pc_epc += 4;
  LDST(exp_state);
}
