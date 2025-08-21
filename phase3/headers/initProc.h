#ifndef INITPROC_H
#define INITPROC_H

#include "../../headers/types.h"

/* Macro per aprire una critical section */
#define CRITICAL_START()                 \
  unsigned int _cs_status = getSTATUS(); \
  setSTATUS(_cs_status& DISABLEINTS)

/* Macro per chiudere una critical section */
#define CRITICAL_END() setSTATUS(_cs_status)

#define UPROC_NUM 8

typedef struct supSem {
  int value;
  int holder_pid;
} supSem;

extern swap_t swap_pool_table[POOLSIZE];
extern supSem swap_pool_sem;
extern supSem sharable_dev_sem[NSUPPSEM];
extern int masterSemaphore;

void getMutex(supSem* sem, int pid);
void releaseMutex(supSem* sem, int pid);
void releaseAllMutex(int pid);

#endif  // INITPROC_H
