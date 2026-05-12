// RUN: %clang %cflags -o %t %s
// RUN: llvm-bolt -o %t.bolt %t 2>&1 | FileCheck %s --check-prefix=WARN
// RUN: llvm-readelf -x .data %t.bolt | FileCheck %s

// WARN-NOT: failed to analyze

  .text
  .globl _start
  .p2align 2
_start:
  // Force BOLT into relocation mode
  .reloc ., R_LARCH_NONE
  li.d  $zero, 0
_test_end:
  ret
  .size _start, .-_start

  .data
// CHECK:      Hex dump of section '.data':
// CHECK-NEXT: 0x{{.*}} 04000000 04000000 00000000
  .reloc ., R_LARCH_ADD32, _test_end
  .reloc ., R_LARCH_SUB32, _start
  .4byte _test_end - _start
  .reloc ., R_LARCH_ADD64, _test_end
  .reloc ., R_LARCH_SUB64, _start
  .8byte _test_end - _start
