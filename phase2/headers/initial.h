#ifndef INITIAL_H
#define INITIAL_H

#include "../../headers/const.h"
#include "../../headers/types.h"
/* Global Variables */
extern int
    process_count; /* number of started, but not yet terminated processes */
extern struct list_head ready_queue; /* queue of PCBs in 'ready' state */
extern pcb_PTR
    current_process[NCPU]; /* pointers to the running PCB on each CPU */
extern semd_t device_semaphores[SEMDEVLEN]; /* array of semaphores, one for each
                                        (sub)device */
extern volatile unsigned int global_lock;   /* 0 or 1 */

#endif
