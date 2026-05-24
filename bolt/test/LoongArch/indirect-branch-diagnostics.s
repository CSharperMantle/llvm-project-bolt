/// Check LoongArch indirect branch matcher diagnostics.

// REQUIRES: asserts

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --print-cfg --debug-only=mcplus -v=1 \
// RUN:   2>&1 | FileCheck %s

// CHECK-DAG: BOLT-DEBUG: failed to match indirect branch: JirlRj has no local def
// CHECK-DAG: BOLT-DEBUG: failed to match LLVM non-PIE jump table: can't resolve scaled index
// CHECK-DAG: BOLT-DEBUG: failed to match LLVM PIC jump table: add.d operand is not ldx.w
// CHECK-DAG: BOLT-DEBUG: failed to match GCC non-PIE jump table: AddrReg def is not alsl.d/add.d

  .text
  .globl _start
  .p2align 2
_start:
  bl unknown_branch
  bl bad_nonpie_index
  bl bad_pic_load
  bl bad_gcc_addr
  ret
  .size _start, .-_start

  .globl unknown_branch
  .p2align 2
unknown_branch:
  jr $a0
  .size unknown_branch, .-unknown_branch

  .globl bad_nonpie_index
  .p2align 2
bad_nonpie_index:
  pcaddi $a1, %pcrel_20(.LJTI_abs)
  ori $a0, $zero, 1
  ldx.d $a2, $a1, $a0
  jr $a2
  .size bad_nonpie_index, .-bad_nonpie_index

  .globl bad_pic_load
  .p2align 2
bad_pic_load:
  pcaddi $a1, %pcrel_20(.LJTI_pic)
  slli.d $a0, $a0, 2
  ldx.d $a2, $a1, $a0
  add.d $a2, $a2, $a1
  jr $a2
  .size bad_pic_load, .-bad_pic_load

  .globl bad_gcc_addr
  .p2align 2
bad_gcc_addr:
  pcaddi $a1, %pcrel_20(.LJTI_abs)
  ld.d $a2, $a1, 0
  jr $a2
  .size bad_gcc_addr, .-bad_gcc_addr

  .section .rodata,"a",@progbits
  .p2align 3
.LJTI_abs:
  .dword unknown_branch
  .dword bad_nonpie_index

  .p2align 2
.LJTI_pic:
  .reloc ., R_LARCH_32_PCREL, unknown_branch
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, bad_pic_load + 4
  .4byte 0
