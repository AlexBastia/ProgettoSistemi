#include "headers/interrupts.h"
#define SAVE_STATE(x) (current_process[x]->p_s = *GET_EXCEPTION_STATE_PTR(x))

#define CAUSE_EXCCODE_BIT 2
#define CAUSE_EXCCODE_MASK1 0x0000007C
#define CAUSE_GET_EXCCODE(x) (((x) &CAUSE_EXCCODE_MASK1) >> CAUSE_EXCCODE_BIT)
//TODO:
//capire dove serve il global lock
//funzione che sblocca i pcb che aspettano lo pseudoclock (line 56)
//capire come leggere i registri del terminale (line 68)
//chiamare correttamente VERHOGEN   FATTO(line 80)

void interruptHandler(state_t* current_state){  
    ACQUIRE_LOCK(&global_lock);
    int int_code = CAUSE_GET_EXCCODE(getCAUSE());
    RELEASE_LOCK(&global_lock);
    int intlineNo = getintLineNo(int_code);
    switch (intlineNo){
        case 1:       pltHandler();               break;
        case 2:       timerHandler(current_state);break;
        case 3 ... 7: intHandler(intlineNo);      break;
        default: break;
    }
}
int getintLineNo(int int_code){
    switch(int_code){
        case IL_CPUTIMER: return 1;
        case IL_TIMER:    return 2;
        case IL_DISK:     return 3;
        case IL_FLASH:    return 4;
        case IL_ETHERNET: return 5;
        case IL_PRINTER:  return 6;
        case IL_TERMINAL: return 7;
        default: return -1;
    }
}
int getdevNo(int intlineNo){
    unsigned int bitmapValue = *((unsigned int*)(INTERRUPT_BITMAP_ADDRESS + (intlineNo - 3) * 4)); //va alla parola giusta della bitmap
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

void pltHandler(){
    setTIMER(TIMESLICE);
    ACQUIRE_LOCK(&global_lock);
    SAVE_STATE(getPRID());
    insertProcQ(&ready_queue, current_process[getPRID()]); 
    RELEASE_LOCK(&global_lock);
    Scheduler();
}

void UNBLOCKALLWAITINGCLOCKPCBS(){}

void timerHandler(state_t* current_state){
    LDIT(PSECOND);
    ACQUIRE_LOCK(&global_lock);
    UNBLOCKALLWAITINGCLOCKPCBS();  /////!!!!!!!!!!!!!!da fare
    pcb_PTR curr = current_process[getPRID()];
    if(curr!=NULL){
        LDST(current_state);
    }
    RELEASE_LOCK(&global_lock);
    Scheduler();
}


void intHandler(int intlineNo){
    int devNo = getdevNo(intlineNo);
    unsigned int devAddrBase = START_DEVREG + ((intlineNo - 3) * 0x80) + (devNo * 0x10); //punto 1
    //come sacrosanta minchia si fa a capire quale dei due subdevice del terminal (con lo schema alla fine del pdf, in device registers) causa l'interrupt????
    if (intlineNo ==7) {
        int TERMINALWRITEINTERRUPT = 0; //placeholder
        if(TERMINALWRITEINTERRUPT){ devAddrBase += 0x8;}
    }
    unsigned int status = *(unsigned int*)devAddrBase; //punto 2
    unsigned int *command = devAddrBase + 0x4;
    *command = ACK; //punto 3
    int deviceID = (devAddrBase - START_DEVREG)/ 0x10;
    int* devSemaphore = &device_semaphores[deviceID];  // get the semaphore of the device
    SYSCALL(VERHOGEN, devSemaphore, 0, 0);//punto 4, V non so come e` implementata
    pcb_PTR pcb = removeBlocked(devSemaphore);
    if(pcb!=NULL){
        pcb->p_s.reg_a0 = status;
        insertProcQ(&ready_queue, pcb);
    } 
    if(current_process[getPRID()]!=NULL){ 
        LDST(status);
    } else {
        Scheduler();
        }
}