// RUN: %clang %cflags -o %t %s
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t | FileCheck %s
// RUN: llvm-objdump -d %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK:      Binary Function "_start" after building cfg {
// CHECK:      bl f

// OBJDUMP:      0000000000{{.*}} <_start>:
// OBJDUMP-NEXT:    bl {{.*}} <f>

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
  bl f
  ret
  .size _start, .-_start
