#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>
#include "initial.h"
#include "exceptions.h"
#include "scheduler.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

#define MIN_NI_line 3 //minimum number of non timer interrupt lines
#define MAX_NI_line 7 //maximum number of non timer interrupt lines
#define INTERRUPT_BITMAP_ADDRESS 0x10000040 //address of interrupt bitmap in memory
/*
Manage the interrupt handling, calling the appropriate handler based on the interrupt line, splitting between timer and non-timer interrupts
@param current_state: the state of the processor when the interrupt occurred
@param cause: the cause of the interrupt
*/
void interruptHandler(state_t* current_state);
int getintLineNo(int int_code);
int getdevNo(int intlineNo);
void pltHandler();
void timerHandler(state_t* current_state);
void intHandler(int intlineNo);
