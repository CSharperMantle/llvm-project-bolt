// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: BOLT-WARNING:
// BOLT: 1 out of 1 functions were overwritten

// OBJDUMP:      {{.*}} <_start>:
// OBJDUMP-NEXT: lu12i.w $t0, 0
// OBJDUMP-NEXT: addi.d  $t0, $t0, 0
// OBJDUMP-NEXT: jr $t0

  .text
  .globl _start
  .p2align 2
_start:
  lu12i.w $t0, 0
  addi.d  $t0, $t0, 0
  jr      $t0
  .size _start, .-_start
