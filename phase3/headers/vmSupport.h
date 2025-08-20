#ifndef VM_SUPPORT_H
#define VM_SUPPORT_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/arch.h>

#include "../../headers/const.h"
#include "../../headers/types.h"

#include "../../phase2/headers/exceptions.h"
#include "initProc.h"

void update_swap_pool_entry(int frame_i, int vpn, int asid, pteEntry_t *pte);

void read_or_write_flash(int frame_i, int vpn, int asid, int op);

void pager();


#endif