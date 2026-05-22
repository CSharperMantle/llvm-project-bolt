/// Check that LongJmp can relax a stub targeting an external LoongArch B26
/// tail-call relocation.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs --unresolved-symbols=ignore-all -pie -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --align-text=0x10000000 2>&1 | FileCheck %s

// CHECK-NOT: PLEASE submit a bug report
// CHECK-NOT: BOLT-ERROR
// CHECK: BOLT-INFO: Inserted 1 stubs in the hot area and 0 stubs in the cold area.

  .text

  .globl main
  .type main, @function
main:
  pcalau12i $a0, %pc_hi20(msg)
  addi.d $a0, $a0, %pc_lo12(msg)
  b printf
  .size main, .-main

  .section .rodata
msg:
  .asciz "x\n"
