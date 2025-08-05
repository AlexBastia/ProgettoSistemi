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
    }
}


static void SYS2() {
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

/**
 * SYS3: WritePrinter
When requested, this service causes the requesting U-proc to be suspended until a line of output
(string of characters) has been transmitted to the printer device associated with the U-proc.
Once the process resumes, the number of characters actually transmitted is returned in a0.
The SYS3 service is requested by the calling U-proc by placing the value 3 in a0, the virtual
address of the first character of the string to be transmitted in a1, the length of this string in a2, and
then executing a SYSCALL instruction. Once the process resumes, the number of characters actually
transmitted is returned in a0 if the write was successful. If the operation ends with a status other
than “Device Ready” (1), the negative of the device’s status value is returned in a0.
It is an error to write to a printer device from an address outside of the requesting U-proc’s logical
address space, request a SYS3 with a length less than 0, or a length greater than 128. Any of these
errors should result in the U-proc being terminated (SYS2).
The following C code can be used to request a SYS3:

int retValue = SYSCALL(WRITEPRINTER, char *virtAddr, int len, 0);

The mnemonic constant WRITEPRINTER has the value of 3.

*/
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