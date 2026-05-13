// RUN: %clang %cflags -o %t %s
// RUN: llvm-bolt %t -o %t.bolt | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: BOLT-WARNING:
// BOLT: BOLT-INFO: Inserted 0 stubs in the hot area and 0 stubs in the cold area.

// OBJDUMP:      {{.*}} <_start>:
// OBJDUMP-NEXT: bl {{.*}} <f>
// OBJDUMP-NEXT: ret

  .text

  .globl f
  .p2align 2
f:
  ret
  .size f, .-f

  .globl _start
  .p2align 2
_start:
  bl f
  ret
  .size _start, .-_start
