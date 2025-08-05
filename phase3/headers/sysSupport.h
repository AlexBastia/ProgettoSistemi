#ifndef SYS_SUPPORT_H
#define SYS_SUPPORT_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/arch.h>

#include "./initProc.h"
#include "./vmSupport.h"

#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"

#include "../../headers/const.h"
#include "../../headers/types.h"

#include "../../phase2/headers/exceptions.h"
#include "../../phase2/headers/initial.h"
#include "../../phase2/headers/scheduler.h"




void syscallHandler(state_t* exp_state);


#endif
