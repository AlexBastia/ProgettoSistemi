
#include "h/tconst.h"
#include <uriscv/liburiscv.h>


int main() {
    // Questo processo non fa nulla se non terminare immediatamente.
    SYSCALL(TERMINATE, 0, 0, 0);
    return 0; // Non verrà mai raggiunto
}

