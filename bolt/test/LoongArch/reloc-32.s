// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -q -o %t %t.o
// RUN: llvm-bolt -o %t.bolt %t
// RUN: llvm-readelf -x .data %t.bolt | FileCheck --check-prefix=DATA %s

// DATA:      Hex dump of section '.data':
// DATA-NEXT: 00004000

  .data
  .globl d
  .p2align 2
d:
  .word _start

  .text
  .globl _start
  .p2align 1
_start:
  ret
  .reloc 0, R_LARCH_NONE
  .size _start, .-_start
