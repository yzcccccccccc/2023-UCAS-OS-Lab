#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la    t0, exception_handler_entry
  csrw  CSR_STVEC, t0
  /* TODO: [p2-task4] enable interrupts globally */
  csrw CSR_SIE, zero       # temporarily diable EIE, TIE, SIE

  li    t0, SR_SIE
  csrs  CSR_SSTATUS, t0
  li    t0, SR_SUM
  csrs  CSR_SSTATUS, t0
  ret
ENDPROC(setup_exception)
