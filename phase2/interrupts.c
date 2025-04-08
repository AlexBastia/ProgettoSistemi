#include "headers/interrupts.h"
#define SAVE_STATE(x) (current_process[x]->p_s = *GET_EXCEPTION_STATE_PTR(x))

/////  GET(x) = x & 0111 1100  shiftato a destra di 2. quindi sono i bit dal terzo al settimo partendo da destra, spostati a destra di 2
/// torna quindi un numero da 0 a 31. 
void interruptHandler(state_t* current_state){  
    ACQUIRE_LOCK(&global_lock);
    int int_code = CAUSE_GET_EXCCODE(getCAUSE());
    int intlineNo = -1;
    switch (int_code) {
        case IL_CPUTIMER:   intlineNo = 1; pltHandler(); break;
        case IL_TIMER:      intlineNo = 2; timerHandler(); break;
        case IL_DISK:       intlineNo = 3; intHandler(findInterruptingDevice(intlineNo, int_code)); break;
        case IL_FLASH:      intlineNo = 4; intHandler(findInterruptingDevice(intlineNo, int_code)); break;
        case IL_ETHERNET:   intlineNo = 5; intHandler(findInterruptingDevice(intlineNo, int_code)); break;
        case IL_PRINTER:    intlineNo = 6; intHandler(findInterruptingDevice(intlineNo, int_code)); break;
        case IL_TERMINAL:   intlineNo = 7; intHandler(findInterruptingDevice(intlineNo, int_code)); break;
        default: ;
    }
    RELEASE_LOCK(&global_lock);
}

void pltHandler(){
    setTIMER(TIMESLICE);
    SAVE_STATE(getPRID());
    insertProcQ(ready_queue, current_process[getPRID()]); //Place the Current Process on the Ready Queue; transitioning the Current Process from the “running” state to the “ready” state

    Scheduler();
}
static int findInterruptingDevice(int int_line, int device_no){
    unsigned int bitmap;

    for(int line = MIN_NI_line; line<=MAX_NI_line; line++){
        bitmap = *((unsigned int *)(INTERRUPT_BITMAP_ADDRESS + (line - 3) * 4));  // Reads the interrupt bitmap

        for (int dev = 0; dev < 8; dev++) {  //up to 8 devices per line
            if (bitmap & (1 << dev)) {  //if the bit is set to 1 the device caused the interrupt
                int_line = line;
                device_no = dev;
                return device_no;
            }
        }
    }
}
void intHandler(int n){

}

void timerHandler(){}

int CAUSE_GET_EXCCODE(int cause){
    return (cause & CAUSE_EXCCODE_MASK) >> 2;
}