#include "headers/interrupts.h"

void interruptHandler(state_t* current_state, unsigned int cause){  
    ACQUIRE_LOCK(&global_lock);
    if(cause == IL_CPUTIMER){
        pltHandler();
    }else if (cause = IL_TIMER){
        timerHandler();
    }else{
        int deviceNumber, int_line_no;
        findInterruptingDevice(&int_line_no, &deviceNumber);
    }
    RELEASE_LOCK(&global_lock);
}

static void findInterruptingDevice(int* int_line, int* device_no){
    unsigned int bitmap;

    for(int line = MIN_NI_line; line<=MAX_NI_line; line++){
        bitmap = *((unsigned int *)(0x10000040 + (line - 3) * 4));  // Reads the interrupt bitmap

        for (int dev = 0; dev < 8; dev++) {  //up to 8 devices per line
            if (bitmap & (1 << dev)) {  //if the bit is set to 1 the device caused the interrupt
                *int_line = line;
                *device_no = dev;
                return;
            }
        }
    }
}