#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void* memcpy(void* dest, const void* src, unsigned int len) {
  char* d = dest;
  const char* s = src;
  while (len--) {
    *d-- = *s++;
  }
  return dest;
}

void initPcbs() {
  struct list_head* pcbst = &pcbFree_h;
  INIT_LIST_HEAD(pcbst);
  for (int i = 0; i < MAXPROC; i++) {
    freePcb(&pcbFree_table[i]);
  }
}

void freePcb(pcb_t* p) { list_add(&p->p_list, &pcbFree_h); }

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
  newPcb->p_pid = next_pid;
  // Ritorna puntatore
  return newPcb;
}

void mkEmptyProcQ(struct list_head* head) { INIT_LIST_HEAD(head); }

int emptyProcQ(struct list_head* head) { return list_empty(head); }

void insertProcQ(struct list_head* head, pcb_t* p) {
  list_add_tail(&p->p_list, head);
}

pcb_t* headProcQ(struct list_head* head) {
  return container_of(list_next(head), pcb_t, p_list);
}

pcb_t* removeProcQ(struct list_head* head) {
  // Dobbiamo rimuovere proprio head (sentinella) oppure il suo next
  // io e il basta facciamo di testa nostra
  if (list_empty(head)) {
    return NULL;
  }
  pcb_t* tmp = headProcQ(head);

  list_del(&tmp->p_list);

  return tmp;
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
  if (p == NULL)
    return NULL;

  else {
    struct list_head* pos;
    list_for_each(pos, head) {
      if (&p->p_list == pos) {
        list_del(
            &p->p_list);  // qui prima era solo list_del(p) quindi dava errore
        return p;
      }
    }
    return NULL;
  }
}

int emptyChild(pcb_t* p) { return list_empty(&p->p_child); }

void insertChild(pcb_t* prnt, pcb_t* p) {
  list_add(&p->p_list, &prnt->p_child);
  p->p_parent =
      prnt;  // imposto il padre del processo che ho appena messo come figlio
}

pcb_t* removeChild(pcb_t* p) {
  if (emptyChild(p)) {
    return NULL;
  }

  struct list_head* child =
      p->p_child.next;  // considero il next della sentinella come primo figlio
  list_del(child);
  pcb_t* removed_child = container_of(child, pcb_t, p_list);
  removed_child->p_parent = NULL;
  return removed_child;
}

pcb_t* outChild(pcb_t* p) {
  if (p->p_parent == NULL) {
    return NULL;
  }

  pcb_t* parent = p->p_parent;
  struct list_head* child = NULL;
  list_for_each(child, &parent->p_child) {
    if (child == &p->p_list) {
      break;
    }
  };

  list_del(child);
  p->p_parent = NULL;
  return p;
}
