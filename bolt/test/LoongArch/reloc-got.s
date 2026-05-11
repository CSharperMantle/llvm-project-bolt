// RUN: %clang %cflags -o %t %s
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.null %t \
// RUN:    | FileCheck %s
// RUN: llvm-objdump -d %t.null | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.null | FileCheck --check-prefix=RELOC %s

  .text

  .global f
  .p2align 1
f:
  ret
  .size f, .-f

  .globl _start
  .p2align 1
// CHECK: Binary Function "_start" after building cfg {
_start:
// CHECK: pcalau12i $t0, %pc_hi20(__BOLT_got_zero+{{[0-9]+}})
// CHECK-NEXT: ld.d $t0, $a2, %pc_lo12(__BOLT_got_zero+{{[0-9]+}})
  pcalau12i $t0, %got_pc_hi20(f)
  ld.d $t0, $a2, %got_pc_lo12(f)
  ret
  .size _start, .-_start

// OBJDUMP:      0000000000400000 <f>:
// OBJDUMP:      0000000000400004 <_start>:
// OBJDUMP-NEXT:     pcalau12i $t0,
// OBJDUMP-NEXT:     ld.d $t0, $a2,
// OBJDUMP-NEXT:     ret

// RELOC: Relocation section '.rela.dyn'
// RELOC: R_LARCH_RELATIVE
