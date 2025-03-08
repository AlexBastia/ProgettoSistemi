#ifndef INITIAL_H
#define INITIAL_H

#include "../../headers/const.h"
#include "../../headers/types.h"

/* Global Variables */
int process_count; /* number of started, but not yet terminated processes */
// ready_queue_type ready_queue; /* queue of PCBs in 'ready' state */ TODO:
// creare struttura dati per la coda ready
pcb_PTR current_process[UPROCMAX];
    /* pointers to the running PCB on each CPU */ /*TODO: capire se mettere NCPU
                                                     o UPROCMAX*/
/* semd_t device_semaphores[###]; TODO: capire che struttura bisogna usare */
int global_lock; /* 0 or 1 */

#endif
