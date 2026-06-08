// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --reorder-blocks=reverse -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// BOLT: basic block reordering modified layout of 1 functions

// OBJDUMP:      0000000000400000 <_start>:
// OBJDUMP-NEXT:     beq $t0, $t1, 20 <_start+0x14>
// OBJDUMP-NEXT:     b 24 <_start+0x1c>
// OBJDUMP-NEXT:     ret
// OBJDUMP-NEXT:     addi.d $a0, $zero, 6
// OBJDUMP-NEXT:     b -8 <_start+0x8>
// OBJDUMP-NEXT:     addi.d $a0, $zero, 5
// OBJDUMP-NEXT:     b -16 <_start+0x8>
// OBJDUMP-NEXT:     beq $t0, $t2, -16 <_start+0xc>
// OBJDUMP-NEXT:     b -12 <_start+0x14>

  .text
  .globl _start
  .p2align 2
_start:
  nop
  .reloc ., R_LARCH_B16, 1f
  beq $t0, $t1, 1f
  nop
  .reloc ., R_LARCH_B16, 2f
  beq $t0, $t2, 2f
1:
  addi.d $a0, $zero, 5
  b 3f
2:
  addi.d $a0, $zero, 6
3:
  ret
  .size _start, .-_start
