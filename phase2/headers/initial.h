#ifndef INITIAL_H
#define INITIAL_H

#include "../../headers/const.h"
#include "../../headers/types.h"
/* Global Variables */
int process_count; /* number of started, but not yet terminated processes */
struct list_head ready_queue;  /* queue of PCBs in 'ready' state */
pcb_PTR current_process[NCPU]; /* pointers to the running PCB on each CPU */
semd_t device_semaphores[SEMDEVLEN]; /* array of semaphores, one for each
                                        (sub)device */
volatile unsigned int global_lock;   /* 0 or 1 */

#endif
