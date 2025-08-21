#include "headers/sysSupport.h"

void generalExceptionSupportHandler() {

    // calling the kernel for the pointer to the support_t structure
    support_t* support = SYSCALL(GETSUPPORTPTR, 0, 0, 0);   //? CAPIRE SE È GIUSTO PRENDERE IL PUNTATORE ALLA STRUTTURA 
                                                            //? SUPPORT_T DAL KERNEL DIO MESCHINI
    state_t* exp_state = &(support->sup_exceptState[GENERALEXCEPT]);

    // get the cause code from the exception state
    unsigned int cause_code = (exp_state->cause & GETEXECCODE) >> CAUSESHIFT; 
    if (cause_code == SYSEXCEPTION) {
        // if the cause code is SYSCALL, call the syscall handler
        syscallSupHandler(exp_state);
    } else {
        // if the cause code is not SYSCALL, call the program trap handler
       programTrapHandler(exp_state);
    }
    
}


static void syscallSupHandler(state_t* exp_state){
    int n_syscall = exp_state->reg_a0;  
    // Il gestore delle eccezioni SYSCALL del Livello di Supporto deve esaminare il valore nel registro a0 (ottenuto dallo saved exception state del processo, che si trova in sup_exceptState della struttura support_t)
    switch(n_syscall) {
        case TERMINATE: {
            // wrapper for the kernel's terminate syscall (NSYS2)
            SYS2(exp_state);
            break;
        }
        case WRITEPRINTER: {
            SYS3(exp_state);
            break;
        }
        case WRITETERMINAL: {
            SYS4(exp_state);
            break;
        }
        case 5: { //! nonostante la consegna impone di utilizzare la costante READTERMINAL (5), questa è assente in headers/const.h oid ocrop, valutare se usare 5 o definire la costante READTERMINAL
            SYS5(exp_state);
            break;
        }
        default: {
            programTrapHandler(exp_state);
            break;

        }
    }
}


/**
 * @brief SYS5: Read a line from the U-proc's dedicated terminal.
 * @param exp_state The saved exception state.
 */
static void SYS5(state_t* exp_state) {
    // ottiene l'indirizzo virtuale del buffer da a1
    char* virtAddr = (char*) exp_state->reg_a1;

    // valida l'indirizzo virtuale del buffer
    if ((unsigned int)virtAddr < UPROCSTARTADDR || (unsigned int)virtAddr >= USERSTACKTOP) {
        programTrapHandler(exp_state);
        return;
    }

    // ottiene l'ASID e l'indice del device
    unsigned int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);
    unsigned int device_index = asid - 1;

    // ottiene l'indirizzo del registro del device terminale
    termreg_t* terminal_device = (termreg_t*) DEV_REG_ADDR(IL_TERMINAL, device_index);

    // prepara i parametri per la chiamata a DOIO del Nucleo
    unsigned int command_address = (unsigned int) &(terminal_device->recv_command);
    unsigned int command_value = RECEIVECHAR;

    // delega l'operazione di I/O e la sincronizzazione al Nucleo
    int status = SYSCALL(DOIO, command_address, command_value, 0);

    // estrae lo stato e il numero di caratteri letti dal valore di ritorno
    unsigned int device_status = status & 0xFF;
    unsigned int chars_read = status >> 8;

    // imposta il valore di ritorno in a0 in base allo stato del device
    if (device_status == 5) {
        // successo: ritorna il numero di caratteri letti
        exp_state->reg_a0 = chars_read;
    } else {
        // errore: ritorna il negativo dello stato del device
        exp_state->reg_a0 = -(int)device_status;
    }

    // avanza il PC per evitare di rieseguire la SYSCALL
    exp_state->pc_epc += 4;

    // ricarica lo stato del processo per riprenderne l'esecuzione
    LDST(exp_state);
}



static void SYS2(state_t* exp_state) {
    // When a U-proc terminates, mark all of the frames it occupied as unoccupied [Section 4.1]. This has the potential to eliminate extraneous writes to the backing store. Technical Point: Since all valid ASID values are positive numbers, one can indicate that a frame is unoccupied with an entry of -1 in that frame’s ASID entry in the Swap Pool table.
    int asid = ENTRYHI_GET_ASID(exp_state->entry_hi);

    for (int i = 0; i < POOLSIZE; i++){
        if(swap_pool_table[i].sw_asid==asid)
           swap_pool_table[i].sw_asid = -1; // segna il frame come non occupato
        
    }
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

static void SYS3(state_t* exp_state) {
    // a2 contains the length of the string to be written
    int len = exp_state->reg_a2;
    // a1 contains the virtual address of the string to be written
    char* virtAddr = exp_state->reg_a1;

    // check if the virtual address is valid
    if ((unsigned int)virtAddr < UPROCSTARTADDR || ((unsigned int)virtAddr + len) > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
        // Invalid address or length, terminate the process
        SYS2();
        return;
    }
    // get the ASID of the current process
    unsigned int entryHi_value = exp_state->entry_hi;
    unsigned int asid = ENTRYHI_GET_ASID(entryHi_value);
    unsigned int device_index = asid - 1; // the printer device is at index ASID-1
    
    // get the address of the printer device register    
    dtpreg_t* printer_device =  (dtpreg_t*) DEV_REG_ADDR(IL_PRINTER, device_index);
    printer_device->data0 = (memaddr) virtAddr;
    printer_device->data1 = len;

    unsigned int command_address = (unsigned int) &(printer_device->command);
    unsigned int command_value = TRANSMITCHAR;
    
    int status = SYSCALL(DOIO, command_address, command_value, 0);
        if (status == READY) {
        // success: return the length of the string written
        exp_state->reg_a0 = len;
    } else {
        // error: return the negative status code
        exp_state->reg_a0 = -status;
    }
    // update the program counter to point to the next instruction
    exp_state->pc_epc += 4;

    LDST(exp_state);
}

static void SYS4(state_t* exp_state) {
    // a2 contiene la lunghezza della stringa da scrivere
    int len = exp_state->reg_a2;
    // a1 contiene l'indirizzo virtuale della stringa da scrivere
    char* virtAddr = (char*)exp_state->reg_a1;

    // Controllo di validità completo e robusto
    if ((unsigned int)virtAddr < UPROCSTARTADDR || ((unsigned int)virtAddr + len) > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
        // Gestione dell'errore centralizzata e sicura
        programTrapHandler(exp_state);
        return;
    }

    // ottiene l'ASID del processo corrente
    unsigned int asID = ENTRYHI_GET_ASID(exp_state->entry_hi);
    unsigned int device_index = asID - 1; // il device terminale è all'indice ASID-1

    // ottiene l'indirizzo del registro del device terminale
    termreg_t* terminal_device = (termreg_t*)DEV_REG_ADDR(IL_TERMINAL, device_index);

    // prepara i parametri per la chiamata a DOIO del Nucleo
    unsigned int command_address = (unsigned int)&(terminal_device->transm_command);
    unsigned int command_value = TRANSMITCHAR;
    
    // delega l'operazione di I/O al Nucleo e ne cattura lo stato
    int status = SYSCALL(DOIO, command_address, command_value, 0);

    // imposta il valore di ritorno per l'U-proc in base allo stato
    if (status == 5) {
        // successo: ritorna il numero di caratteri scritti
        exp_state->reg_a0 = len;
    } else {
        // errore: ritorna il negativo dello stato
        exp_state->reg_a0 = -status;
    }

    // avanza il PC per evitare di rieseguire la SYSCALL
    exp_state->pc_epc += 4;

    // ricarica lo stato del processo per riprenderne l'esecuzione
    LDST(exp_state);
}

// The Program Trap Exception Handler
// For all Program Trap exceptions, the nucleus either treats the exception as a process termination or
// “passes up” the handling of the exception if the offending process was provided a non-NULL value for
// its Support Structure pointer when it was created.
// Assuming that the handling of the exception is to be passed up (non-NULL Support Structure
// pointer) and the appropriate sup_exceptContext fields of the Support Structure were correctly ini-
// tialized, execution continues with the Support Level’s general exception handler, which should then
// pass control to the Support Level’s Program Trap exception handler. The processor state at the time
// of the exception will be in the Support Structure’s corresponding sup_exceptState field.
// The Support Level’s Program Trap exception handler is to terminate the process in an orderly
// fashion; perform the same operations as a SYS2 request.
// Important: If the process to be terminated is currently holding mutual exclusion on a Support
// Level semaphore (e.g. Swap Pool semaphore), mutual exclusion must first be released (NSYS4) before
// invoking the Nucleus terminate command (NSYS2).

