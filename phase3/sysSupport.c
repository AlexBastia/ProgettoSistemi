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
        // if the cause code is not SYSCALL, pass up or die
       //TODO
    }
    
}

/*
7.4
ReadTerminal (SYS5)
When requested, this service causes the requesting U-proc to be suspended until a line of input (string
of characters) has been transmitted from the terminal device associated with the U-proc.
The SYS5 service is requested by the calling U-proc by placing the value 5 in a0, the virtual
address of a string buffer where the data read should be placed in a1, and then executing a SYSCALL
instruction. Once the process resumes, the number of characters actually transmitted is returned in
a0 if the read was successful. If the operation ends with a status other than “Character Received”
(5), the negative of the device’s status value is returned in a0.
Attempting to read from a terminal device to an address outside of the requesting U-proc’s logical
address space is an error and should result in the U-proc being terminated (SYS2).
The following C code can be used to request a SYS5:
int retValue = SYSCALL(READTERMINAL, char *virtAddr, 0, 0);
Where the mnemonic constant READTERMINAL has the value of 5
*/

static void syscallSupHandler(state_t* exp_state){
    int n_syscall = exp_state->reg_a0;  
    // Il gestore delle eccezioni SYSCALL del Livello di Supporto deve esaminare il valore nel registro a0 (ottenuto dallo saved exception state del processo, che si trova in sup_exceptState della struttura support_t)
    switch(n_syscall) {
        case TERMINATE: {
            // wrapper for the kernel's terminate syscall (NSYS2)
            SYS2();
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
        case READTERMINAL: { //! nonostante la consegna impone di utilizzare la costante READTERMINAL (5), questa è assente in headers/const.h oid ocrop, valutare se usare 5 o definire la costante READTERMINAL
            SYS5(exp_state);
            break;
        }
        default: {
            // if the syscall is not recognized, pass up or die
            //TODO
        }
    }
}


static void SYS5(state_t* exp_state){
    // a2 contains the length of the string to be written
    int len = exp_state->reg_a2;
    // a1 contains the virtual address of the string to be written
    char* virtAddr = exp_state->reg_a1;

        // check if the virtual address is valid
    if (virtAddr == NULL || (unsigned int)virtAddr > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
        // ? la consegna è utilizare SYS2 per terminare il processo, ma capire se intende letteralmente oppure chiamare un trap per terminare il processo (stesso servizio di SYS2). GPT mi dice che l'archietettura corretta è di terminare il processo chiamando un trap
        SYS2();
        return;
    }

        // Ottieni ASID e indice del dispositivo
    unsigned int entryHi_value = exp_state->entry_hi;
    unsigned int asid = (entryHi_value >> ASIDSHIFT) & 0x3F;
    unsigned int device_index = asid - 1;

    // Ottieni indirizzo del registro del dispositivo terminale
    dtpreg_t* terminal_device = (dtpreg_t*) DEV_REG_ADDR(IL_TERMINAL, device_index);

    // Imposta i campi data0 e data1 del registro del dispositivo
    terminal_device->data0 = (memaddr) virtAddr;
    terminal_device->data1 = len;

    // Prepara comando per DOIO (leggere dal terminale)
    unsigned int command_address = (unsigned int) &(terminal_device->command);
    unsigned int command_value = RECEIVECHAR;

    // Esegue operazione I/O
    SYSCALL(DOIO, command_address, command_value, 0);

    // Riprendi esecuzione del processo
    LDST(exp_state);

    
}




static void SYS2() {
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

static void SYS3(state_t* exp_state) {

    // a2 contains the length of the string to be written
    int len = exp_state->reg_a2;
    // a1 contains the virtual address of the string to be written
    char* virtAddr = exp_state->reg_a1;

    // check if the virtual address is valid
    if (virtAddr == NULL || (unsigned int)virtAddr > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
        // ? la consegna è utilizare SYS2 per terminare il processo, ma capire se intende letteralmente oppure chiamare un trap per terminare il processo (stesso servizio di SYS2). GPT mi dice che l'archietettura corretta è di terminare il processo chiamando un trap
        SYS2();
        return;
    }
    // get the ASID of the current process
    unsigned int entryHi_value = exp_state->entry_hi;
    unsigned int asid = (entryHi_value >> ASIDSHIFT) & 0x3F; //? capire se è corretto il calcolo dell'ASID del processo corrente
    unsigned int device_index = asid - 1; // the printer device is at index ASID-1

    // get the address of the printer device register    
    dtpreg_t* printer_device =  (dtpreg_t*) DEV_REG_ADDR(IL_PRINTER, device_index);
    printer_device->data0 = (memaddr) virtAddr;
    printer_device->data1 = len;

    unsigned int command_address = (unsigned int) &(printer_device->command);
    unsigned int command_value = TRANSMITCHAR;
    
    SYSCALL(DOIO, command_address, command_value, 0);

    LDST(exp_state);
}

static void SYS4(state_t* exp_state) {
    // a2 contains the length of the string to be written
    int len = exp_state->reg_a2;
    // a1 contains the virtual address of the string to be written
    char* virtAddr = exp_state->reg_a1;

    // check if the virtual address is valid
    if (virtAddr == NULL || (unsigned int)virtAddr > USERSTACKTOP || len < 0 || len > MAXSTRLENG) {
        // ? la consegna è utilizare SYS2 per terminare il processo, ma capire se intende letteralmente oppure chiamare un trap per terminare il processo (stesso servizio di SYS2). GPT mi dice che l'archietettura corretta è di terminare il processo chiamando un trap
        SYS2();
        return;
    }
    // get the ASID of the current process
    unsigned int entryHi_value = exp_state->entry_hi;
    unsigned int asid = (entryHi_value >> ASIDSHIFT) & 0x3F; //? capire seè corretto il calcolo dell'ASID del processo corrente
    unsigned int device_index = asid - 1; // the printer device is at index ASID-1 in the device_semaphores array    

    // get the address of the printer device register    
    dtpreg_t* printer_device =  (dtpreg_t*) DEV_REG_ADDR(IL_PRINTER, device_index);
    printer_device->data0 = (memaddr) virtAddr;
    printer_device->data1 = len;

    unsigned int command_address = (unsigned int) &(printer_device->command);
    unsigned int command_value = TRANSMITCHAR;
    
    SYSCALL(DOIO, command_address, command_value, 0);

    LDST(exp_state);
}
