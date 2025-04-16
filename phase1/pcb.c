#include "./headers/pcb.h"
static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 0;

void* memcpy(void* dest, const void* src, unsigned int len) {
  char* d = dest;
  const char* s = src;
  while (len--) {
    *d++ = *s++; //prima era *d--
  }
  return dest;
}

// Initialize the pcbFree list to contain all the elements of the static array
// of MAXPROC PCBs. This method will be called only once during data structure
// initialization.
void initPcbs() {
  struct list_head* pcbst = &pcbFree_h;
  INIT_LIST_HEAD(pcbst);
  for (int i = 0; i < MAXPROC; i++) {
    freePcb(&pcbFree_table[i]);
  }
}

// Insert the element pointed to by p onto the pcbFree list.
void freePcb(pcb_t* p) { list_add(&p->p_list, &pcbFree_h); }

// Return NULL if the pcbFree list is empty. Otherwise, remove an element from
// the pcbFree list, provide initial values for ALL of the PCBs fields (i.e.
// NULL and/or 0) and then return a pointer to the removed element. PCBs get
// reused, so it is important that no previous value persist in a PCB when it
// gets reallocated.
pcb_t* allocPcb() {
  // Return NULL if pcbFree_h is emtpy
  if (list_empty(&pcbFree_h)) return NULL;

  // Remove from pcbFree_h
  struct list_head* removed = pcbFree_h.next;
  list_del(removed);
  // dato che *removed è un puntatore a list_head uso la macro container of per
  // trovar il puntatore a pcb_t
  pcb_t* newPcb = container_of(removed, pcb_t, p_list);
  // Inizializza tutti i campi (default)
  INIT_LIST_HEAD(&newPcb->p_list);

  // notare che il campo padre è l'unico che è un semplice puntatore a pcb_t
  // invece di una lista
  newPcb->p_parent = NULL;

  INIT_LIST_HEAD(&newPcb->p_child);
  INIT_LIST_HEAD(&newPcb->p_sib);

  newPcb->p_time = (int)NULL;
  newPcb->p_semAdd = NULL;
  newPcb->p_supportStruct = NULL;
  newPcb->p_pid = ++next_pid;
  newPcb->p_s.cause = 0;
  newPcb->p_s.entry_hi = 0;
  newPcb->p_s.status = 0;
  newPcb->p_s.pc_epc = 0;
  
  for (int i = 0; i < STATE_GPR_LEN; i++) {
    newPcb->p_s.gpr[i] = 0;
  }
  // Ritorna puntatore
  return newPcb;
}

// This method is used to initialize a variable to be head pointer
// to a process queue.
void mkEmptyProcQ(struct list_head* head) { INIT_LIST_HEAD(head); }

// Return TRUE if the queue whose head is pointed to by head is empty.
// Return FALSE otherwise.
int emptyProcQ(struct list_head* head) { return list_empty(head); }

// Insert the PCB pointed by p into the process queue
// whose head pointer is pointed to by head.
void insertProcQ(struct list_head* head, pcb_t* p) {
  list_add_tail(&p->p_list, head);
}

// Return a pointer to the first PCB from the process
// queue whose head is pointed to by head.
// Do not remove this PCB from the process queue.
// Return NULL if the process queue is empty.
pcb_t* headProcQ(struct list_head* head) {
  return container_of(
      list_next(head), pcb_t,
      p_list);  // list_next ritrona puntatore a list_head quindi utilizzo
                // container_of per ottenere il puntatore a pcb_t
}

// Remove the first (i.e. head) element from the process queue whose head
// pointer is pointed to by head. Return NULL if the process queue was initially
// empty; otherwise return the pointer to the removed element.
pcb_t* removeProcQ(struct list_head* head) {
  if (list_empty(head)) {
    return NULL;
  }

  pcb_t* tmp = headProcQ(head);
  list_del(&tmp->p_list);

  return tmp;
}

// Remove the PCB pointed to by p from the process queue whose
// head pointer is pointed to by head. If the desired entry is
// not in the indicated queue (an error condition), return NULL;
// otherwise, return p. Note that p can point to any element of the
// process queue.
pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
  if (p == NULL)
    return NULL;

  else {
    struct list_head* pos;
    list_for_each(pos, head) {
      if (&p->p_list == pos) {
        list_del(&p->p_list);
        return p;
      }
    }
    return NULL;
  }
}

// Return TRUE if the PCB pointed to by p has no children. Return FALSE
// otherwise.
int emptyChild(pcb_t* p) { return list_empty(&p->p_child); }

// Make the PCB pointed to by p a child of the PCB pointed to by prnt
void insertChild(pcb_t* prnt, pcb_t* p) {
  list_add(&p->p_sib, &prnt->p_child);
  p->p_parent =
      prnt;  // imposto il padre del processo che ho appena messo come figlio
}

// Make the first child of the PCB pointed to by p no longer a child of p.
// Return NULL if initially there were no children of p. Otherwise, return a
// pointer to this removed first child PCB.
pcb_t* removeChild(pcb_t* p) {
  if (emptyChild(p)) {
    return NULL;
  }

  struct list_head* child =
      p->p_child.next;  // considero il next della sentinella come primo figlio
  list_del(child);
  pcb_t* removed_child = container_of(child, pcb_t, p_sib);
  removed_child->p_parent = NULL;  // visto  che non ha più un padre
  return removed_child;
}

// Make the PCB pointed to by p no longer the child of its parent. If the PCB
// pointed to by p has no parent, return NULL; otherwise, return p. Note that
// the element pointed to by p could be in an arbitrary position (i.e. not be
// the first child of its parent).
pcb_t* outChild(pcb_t* p) {
  if (p->p_parent == NULL) {  // se il processo non ha un padre
    return NULL;              // ritorno NULL
  }

  pcb_t* parent = p->p_parent;     // padre del processo
  struct list_head* child = NULL;  // inizializzo a NULL
  list_for_each(child,
                &parent->p_child) {  // scorro la lista dei figli del padre
    if (child == &p->p_sib) {        // se trovo il figlio
      break;                         // esco dal ciclo
    }
  };

  list_del(child);     // rimuovo il figlio
  p->p_parent = NULL;  // il figlio non ha più un padre
  return p;
}