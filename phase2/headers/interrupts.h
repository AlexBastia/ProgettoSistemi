#include <uriscv/liburiscv.h>
#include "initial.h"
#include <uriscv/cpu.h>

#define MIN_NI_line 3 //minimum number of non timer interrupt lines
#define MAX_NI_line 7 //maximum number of non timer interrupt lines
#define INTERRUPT_BITMAP_ADDRESS 0x10000040 //address of interrupt bitmap in memory
/*
Manage the interrupt handling, calling the appropriate handler based on the interrupt line, splitting between timer and non-timer interrupts
@param current_state: the state of the processor when the interrupt occurred
@param cause: the cause of the interrupt
*/
void interruptHandler(state_t* current_state);

/*
Find the device that caused the interrupt
@param int_line: the interrupt line that caused the interrupt
@param device_no: the device that caused the interrupt
*/
static int findInterruptingDevice(int int_line, int device_no);