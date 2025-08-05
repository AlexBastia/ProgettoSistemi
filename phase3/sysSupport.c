/*
/*
The SYSCALL Exception Handler
The nucleus directly handles all NSYS SYSCALL exceptions (those having negative identifiers). For
all other SYSCALL exceptions the nucleus either treats the exception as a NSYS2 (terminate) or
“passes up” the handling of the exception if the offending process was provided a non-NULL value for
its Support Structure pointer when it was created.
Assuming that the handling of the exception is to be passed up (non-NULL Support Structure
pointer) and the appropriate sup_exceptContext fields of the Support Structure were correctly ini-
tialized, execution continues with the Support Level’s general exception handler, which should then
pass control to the Support Level’s SYSCALL exception handler. The processor state at the time of
the exception will be in the Support Structure’s corresponding sup_exceptState field.
9By convention the executing process places appropriate values in the general purpose registers
a0–a3 immediately prior to executing the SYSCALL instruction. The Support Level’s SYSCALL
exception handler will then perform some service on behalf of the U-proc executing the SYSCALL
instruction depending on the value found in a0.
Upon successful completion of a SYSCALL request any return status is placed in a0, and control
is returned to the calling process at the instruction immediately following the SYSCALL instruction.
Similar to what the Nucleus does when returning from a successful SYSCALL request, the Support
Level’s SYSCALL exception handler must also increment the PC by 4 in order to return control to
the instruction after the SYSCALL instruction.
In particular, if a U-proc executes a SYSCALL instruction and a0 contained a valid positive value
then the Support Level should perform one of the services described below.
7.1
Terminate (SYS2)
This services causes the executing U-proc to cease to exist. The SYS2 service is essentially a user-mode
“wrapper” for the kernel-mode restricted NSYS2 service.
The SYS2 service is requested by the calling process by placing the value 2 in a0 and then executing
a SYSCALL instruction.
The following C code can be used to request a SYS2:
SYSCALL(TERMINATE, 0, 0, 0);
The mnemonic constant TERMINATE has the value of 2.
7.2
WritePrinter (SYS3)
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
7.3
WriteTerminal (SYS4)
When requested, this service causes the requesting U-proc to be suspended until a line of output
(string of characters) has been transmitted to the terminal device associated with the U-proc.
The SYS4 service is requested by the calling U-proc by placing the value 4 in a0, the virtual
address of the first character of the string to be transmitted in a1, the length of this string in a2, and
then executing a SYSCALL instruction. Once the process resumes, the number of characters actually
transmitted is returned in a0 if the write was successful. If the operation ends with a status other
than “Character Transmitted” (5), the negative of the device’s status value is returned in a0.
10It is an error to write to a terminal device from an address outside of the requesting U-proc’s
logical address space, request a SYS4 with a length less than 0, or a length greater than 128. Any of
these errors should result in the U-proc being terminated (SYS2).
The following C code can be used to request a SYS4:
int retValue = SYSCALL(WRITETERMINAL, char *virtAddr, int len, 0);
The mnemonic constant WRITETERMINAL has the value of 4.
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
Where the mnemonic constant READTERMINAL has the value of 5.

Dopo che il GeneralExceptionHandler ha identificato un'eccezione di tipo SYSCALL e ti ha passato il controllo, è compito del SYSCALL Exception Handler occuparsi della richiesta specifica.

Questo gestore è l'interfaccia di servizio del tuo sistema operativo. È l'ufficio a cui i processi utente si rivolgono per chiedere di compiere azioni che non possono fare da soli, come terminare l'esecuzione o interagire con i dispositivi hardware.

Il gestore viene invocato dal GeneralExceptionHandler e la sua prima azione è esaminare il registro a0 (salvato nell'area di stato dell'eccezione) per capire quale servizio è stato richiesto.

La Struttura del Gestore: Lo switch
La maniera più pulita e logica per implementare questo gestore in sysSupport.c è tramite una struttura switch basata sul valore di a0.

C

// In sysSupport.c
void syscallHandler(state_t* exception_state) {
    // Leggi il numero della SYSCALL da a0
    int syscall_number = exception_state->a0;

    switch (syscall_number) {
        case 2:
            // Gestisci SYS2 (Terminate)
            break;
        
        case 3:
            // Gestisci SYS3 (WritePrinter)
            break;

        // ... altri casi ...

        default:
            // SYSCALL non riconosciuta -> Program Trap
            programTrapHandler(exception_state);
            break;
    }
}
Ora analizziamo in dettaglio cosa fare per ogni case.

Analisi Dettagliata dei Servizi
case 2: SYS2 (Terminate)
Scopo: Permettere al processo utente di terminare la propria esecuzione.

Parametri: Nessuno.

Logica di Implementazione: Questo è il servizio più semplice. È un "wrapper" user-mode per il servizio NSYS2 del nucleo.

Chiama NSYS2();

Non c'è bisogno di fare altro. Il processo non tornerà mai da questa chiamata, quindi non devi preoccuparti di incrementare il PC o di ritornare. Il Nucleo si occuperà di distruggere il processo.

case 3: SYS3 (WritePrinter) e case 4: SYS4 (WriteTerminal)
La logica per questi due servizi è quasi identica, cambia solo il dispositivo di destinazione.

Scopo: Scrivere una stringa di caratteri sul dispositivo (stampante o terminale) associato al processo.

Parametri:

a1: L'indirizzo virtuale della stringa da scrivere.

a2: La lunghezza della stringa.

Logica di Implementazione:

Validazione Parametri: Questo passo è CRUCIALE. Devi assicurarti che l'indirizzo passato in a1 sia un indirizzo valido nello spazio utente (kuseg). Un controllo semplice è verificare che sia inferiore a $C000.0000. Se il puntatore non è valido, il processo sta cercando di fare qualcosa di male. Non procedere! Invece, invoca immediatamente il ProgramTrapHandler per terminarlo.

Identifica il Dispositivo: Determina il dispositivo corretto in base all'ASID del processo (che trovi nella support_t). La stampante sarà la numero ASID-1 e il terminale sarà il numero ASID-1.

Acquisisci il Semaforo del Dispositivo: Esegui una P() sul semaforo del dispositivo scelto (NSYS3). Questo mette in attesa il processo se il dispositivo è occupato e garantisce l'accesso esclusivo.

Esegui l'Operazione di I/O: Chiama NSYS5 per avviare la scrittura. Dovrai passare a NSYS5 l'indirizzo del registro del dispositivo, il comando di scrittura e i dati (l'indirizzo e la lunghezza della stringa).

Memorizza il Risultato: NSYS5 ritornerà uno status (es. il numero di caratteri scritti). Salva questo valore.

Rilascia il Semaforo del Dispositivo: Esegui una V() sul semaforo (NSYS4) per rendere di nuovo disponibile il dispositivo.

Imposta il Valore di Ritorno: Scrivi il risultato dell'operazione di I/O (salvato al punto 5) nel registro a0 dell'exception_state. In questo modo, quando il processo utente riprenderà l'esecuzione, troverà il risultato in a0.

case 5: SYS5 (ReadTerminal)
Scopo: Leggere una riga di input dal terminale associato al processo.

Parametri:

a1: L'indirizzo virtuale del buffer dove memorizzare la stringa letta.

Logica di Implementazione: Segue lo stesso schema della scrittura.

Validazione Parametri: Come prima, controlla che l'indirizzo del buffer in a1 sia valido. Se non lo è, invoca il ProgramTrapHandler.

Identifica il Dispositivo: Terminale ASID-1.

Acquisisci il Semaforo: NSYS3 sul semaforo del terminale (lato lettura).

Esegui l'Operazione di I/O: NSYS5 per avviare la lettura.

Memorizza il Risultato: Salva lo status/numero di caratteri letti.

Rilascia il Semaforo: NSYS4 sul semaforo.

Imposta il Valore di Ritorno: Scrivi il risultato in a0 dell'exception_state.

Il Ritorno al Processo Utente: Il Passo più Importante
Dopo aver gestito una SYSCALL che non termina il processo (quindi tutte tranne SYS2), devi fare due cose per restituire il controllo correttamente:

Incrementare il Program Counter (PC): L'istruzione syscall occupa 4 byte. Se non incrementi il PC, al ritorno il processo eseguirà di nuovo la stessa istruzione syscall, creando un loop infinito.

Azione: exception_state->pc = exception_state->pc + 4;

Ricaricare lo Stato: Esegui LDST sull'indirizzo dell'area di stato dell'eccezione per ripristinare il contesto del processo utente e farlo ripartire dall'istruzione successiva alla syscall.

Azione: LDST(exception_state);

Se dimentichi di incrementare il PC, il tuo sistema si bloccherà apparentemente senza motivo non appena un processo utente esegue la sua prima SYSCALL di I/O. Fai molta attenzione a questo dettaglio!

*/

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


    // PASSO 4: Ritorno al processo utente.
    // Quando il processo si risveglia qui, il suo lavoro in kernel mode è finito.
    // L'interrupt handler ha già messo il risultato dell'operazione in a0.
    // Dobbiamo solo ricaricare il suo stato.
    LDST(exp_state);
}