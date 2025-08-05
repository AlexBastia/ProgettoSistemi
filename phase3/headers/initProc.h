#ifndef INITPROC_H
#define INITPROC_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"

#include "../../headers/const.h"
#include "../../headers/types.h"

#include "../../phase2/headers/exceptions.h"
#include "../../phase2/headers/initial.h"
#include "../../phase2/headers/scheduler.h"


extern unsigned int dev_semaphores[NRSEMAPHORES];


#endif// INITPROC_H
