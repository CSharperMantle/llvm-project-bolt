// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.null %t | FileCheck %s

// CHECK-LABEL: Binary Function "_start" after building cfg {
// CHECK:       IsSimple    : 1
// CHECK:       IsMultiEntry: 0
// CHECK:       addi.d $a0, $zero, 0
// CHECK-NEXT:  ret

  .text
  .globl _start
  .p2align 2
_start:
  addi.d $a0, $zero, 0
  ret
  .size _start, .-_start
