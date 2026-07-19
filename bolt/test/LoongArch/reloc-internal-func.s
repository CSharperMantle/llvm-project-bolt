// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --reorder-blocks=reverse -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// BOLT: basic block reordering modified layout of 1 functions

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

  .globl f
  .p2align 2
f:
  beq $t0, $t1, 1f
  pcalau12i $a0, %pc_hi20(g)
  addi.d $a0, $a0, %pc_lo12(g)
  ret
1:
  ori $a0, $zero, 1
  ret
  .size f, .-f

  .globl g
  .p2align 2
g:
  ret
  .size g, .-g

// OBJDUMP:      <f>:
// OBJDUMP-NEXT:     bne $t0, $t1, 12 <f+0xc>
// OBJDUMP-NEXT:     ori $a0, $zero, 1
// OBJDUMP-NEXT:     ret
// OBJDUMP-NEXT:     pcalau12i $a0,
// OBJDUMP-NEXT:     addi.d $a0, $a0,
// OBJDUMP-NEXT:     ret
