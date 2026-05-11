/// Test that no secondary entry points are created for basic block labels used
/// by branches.
// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.null %t | FileCheck %s

// CHECK-LABEL: Binary Function "_start" after building cfg {
// CHECK:       IsMultiEntry: 0
// CHECK:       beq $t0, $t1, .Ltmp0
// CHECK:       {{^}}.Ltmp0
// CHECK:       ret

  .text
  .globl _start
  .p2align 2
_start:
  beq $t0, $t1, 1f
1:
  ret
  .size _start, .-_start
