// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.null %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.null | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.null | FileCheck --check-prefix=RELOC %s

// CHECK-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start" after building cfg {
// CHECK:       IsSimple    : 1
// CHECK:       IsMultiEntry: 0
// CHECK:       bnez $t2, .Ltmp0
// OBJDUMP:      0000000000400000 <_start>:
// OBJDUMP-NEXT:     bnez $t2, 8 <_start+0x8>
// OBJDUMP-NEXT:     addi.d $a0, $zero, 1
// OBJDUMP-NEXT:     addi.d $a0, $zero, 2
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.

  .text
  .globl _start
  .p2align 2
_start:
  .reloc ., R_LARCH_B21, 1f
  bnez $t2, 1f
  addi.d $a0, $zero, 1
1:
  addi.d $a0, $zero, 2
  ret
  .size _start, .-_start
