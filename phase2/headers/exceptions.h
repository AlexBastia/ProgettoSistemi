#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"

// exception handler gets the cause of the exception and calls the appropriate handler
void exceptionHandler(void);
int findDeviceIndex(memaddr*);

#endif
