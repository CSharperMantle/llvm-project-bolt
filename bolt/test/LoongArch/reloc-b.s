// RUN: %clang %cflags -o %t %s
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t | FileCheck %s
// RUN: llvm-objdump -d %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK:      Binary Function "_start" after building cfg {
// CHECK:      b .Ltmp0
// CHECK:      b f # TAILCALL

// OBJDUMP:      {{.*}} <_start>:
// OBJDUMP:      b {{.*}} <f>

// RELOC: There are no relocations in this file.

  .text

  .global f
  .p2align 1
f:
  ret
  .size f, .-f

  .globl _start
  .p2align 1
_start:
  b 1f
1:
  b f
  ret
  .size _start, .-_start
