#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
  LIST_HEAD(pcbFree_h);
  for (int i = 0; i < MAXPROC; i++) {
    freePcb(&pcbFree_table[i]);
  }
}

void freePcb(pcb_t* p) { list_add(&p->p_list, &pcbFree_h); }

pcb_t* allocPcb() {
  // Return NULL if pcbFree_h is emtpy
  if (list_empty(&pcbFree_h)) return NULL;

  // Remove from pcbFree_h

  // Inizializza tutti i campi (default)

  // Ritorna puntatore
}

void mkEmptyProcQ(struct list_head* head) {}

int emptyProcQ(struct list_head* head) {}

void insertProcQ(struct list_head* head, pcb_t* p) {}

pcb_t* headProcQ(struct list_head* head) {}

pcb_t* removeProcQ(struct list_head* head) {}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {}

int emptyChild(pcb_t* p) {}

void insertChild(pcb_t* prnt, pcb_t* p) {}

pcb_t* removeChild(pcb_t* p) {}

pcb_t* outChild(pcb_t* p) {}
