#ifndef INITPROC_H
#define INITPROC_H

#include "../../headers/types.h"

extern unsigned int dev_semaphores[NRSEMAPHORES];  // a chi serve sta roba? giuro che non mi ricordo se l'ho scritto io

#define UPROC_NUM 8
extern swap_t swap_pool_table[POOLSIZE];
extern int swap_pool_sem;
extern int sharable_dev_sem[NSUPPSEM];
extern int masterSemaphore;

#endif  // INITPROC_H
