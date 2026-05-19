// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --relocs --strict --print-cfg --print-only=stack_index -o %t.stack-index %t 2>&1 | FileCheck --check-prefix=STACK-INDEX %s
// RUN: llvm-bolt --relocs --strict --print-cfg --print-only=spilled_base -o %t.spilled-base %t 2>&1 | FileCheck --check-prefix=SPILLED-BASE %s
// RUN: llvm-bolt --relocs --strict --print-cfg --print-only=spilled_target -o %t.spilled-target %t 2>&1 | FileCheck --check-prefix=SPILLED-TARGET %s

// STACK-INDEX-NOT: unclaimed PC-relative relocation
// STACK-INDEX: JUMPTABLE @
// STACK-INDEX: Jump table {{.+}} for function stack_index

// SPILLED-BASE-NOT: unclaimed PC-relative relocation
// SPILLED-BASE: JUMPTABLE @
// SPILLED-BASE: Jump table {{.+}} for function spilled_base

// SPILLED-TARGET-NOT: unclaimed PC-relative relocation
// SPILLED-TARGET: JUMPTABLE @
// SPILLED-TARGET: Jump table {{.+}} for function spilled_target

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

  .globl stack_index
  .p2align 2
stack_index:
  pcaddi $a0, %pcrel_20(.LJTI_stack_index)
  ld.d   $a1, $sp, 0
  ldx.w  $a1, $a0, $a1
  add.d  $a0, $a0, $a1
  jr     $a0
.Lstack_index_0:
  ret
.Lstack_index_1:
  ret
  .size stack_index, .-stack_index

  .globl spilled_base
  .p2align 2
spilled_base:
  pcaddi $a0, %pcrel_20(.LJTI_spilled_base)
  st.d   $a0, $sp, 16
  ld.d   $a1, $sp, 16
  ori    $a2, $zero, 1
  slli.d $a2, $a2, 2
  ldx.w  $a2, $a1, $a2
  add.d  $a1, $a1, $a2
  jr     $a1
.Lspilled_base_0:
  ret
.Lspilled_base_1:
  ret
  .size spilled_base, .-spilled_base

  .globl spilled_target
  .p2align 2
spilled_target:
  ori    $a1, $zero, 1
  slli.d $a1, $a1, 2
  pcaddi $a0, %pcrel_20(.LJTI_spilled_target)
  ldx.w  $a1, $a0, $a1
  add.d  $a0, $a0, $a1
  st.d   $a0, $sp, 24
  ld.d   $a0, $sp, 24
  jr     $a0
.Lspilled_target_0:
  ret
.Lspilled_target_1:
  ret
  .size spilled_target, .-spilled_target

  .section .rodata,"a",@progbits
  .p2align 2
.LJTI_stack_index:
  .reloc ., R_LARCH_32_PCREL, .Lstack_index_0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Lstack_index_1 + 4
  .4byte 0

  .p2align 2
.LJTI_spilled_base:
  .reloc ., R_LARCH_32_PCREL, .Lspilled_base_0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Lspilled_base_1 + 4
  .4byte 0

  .p2align 2
.LJTI_spilled_target:
  .reloc ., R_LARCH_32_PCREL, .Lspilled_target_0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Lspilled_target_1 + 4
  .4byte 0
