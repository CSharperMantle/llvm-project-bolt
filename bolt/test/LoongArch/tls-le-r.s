// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-readelf -r %t | FileCheck --check-prefix=READELF %s
// RUN: llvm-bolt %t -o %t.bolt 2>&1 | FileCheck %s

// CHECK-NOT: BOLT-WARNING: Failed to analyze
// READELF: R_LARCH_TLS_LE_HI20_R
// READELF: R_LARCH_TLS_LE_LO12_R

  .text
  .globl _start
  .p2align 2
_start:
  lu12i.w $a0, %le_hi20_r(x)
  addi.d  $a0, $a0, %le_lo12_r(x)
  add.d   $a0, $tp, $a0
  li.d    $a0, 0
  li.d    $a7, 93
  syscall 0
  .size _start, .-_start

  .section .tbss,"awT",@nobits
  .globl x
  .p2align 2
x:
  .word 0
