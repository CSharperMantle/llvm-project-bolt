/// Check LoongArch virtual method call matcher diagnostics.

// REQUIRES: asserts

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs -o %t %t.o
// RUN: link_fdata %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --icp=calls \
// RUN:   --icp-top-callsites=0 --icp-calls-topn=1 --icp-eliminate-loads \
// RUN:   --debug-only=mcplus -v=1 2>&1 | FileCheck %s

// CHECK: BOLT-DEBUG: failed to match virtual method call: method load is not fixed-slot ld.d/ldptr.d

  .text
  .globl _start
  .p2align 2
_start:
  bl caller_ldx
  ret
  .size _start, .-_start

  .globl caller_ldx
  .p2align 2
caller_ldx:
  pcalau12i $a0, %pc_hi20(obj)
  ld.d $a0, $a0, %pc_lo12(obj)
  ld.d $a0, $a0, 0
  ori $a2, $zero, 16
  ldx.d $a1, $a0, $a2
  jirl $ra, $a1, 0
// FDATA: 1 caller_ldx 14 1 hot 0 0 100
// FDATA: 4 caller_ldx 10 4 vtable #vtable_method# 100
  ret
  .size caller_ldx, .-caller_ldx

  .globl hot
  .p2align 2
hot:
  ret
  .size hot, .-hot

  .data
  .globl obj
obj:
  .dword vtable

  .section .rodata,"a",@progbits
  .globl vtable
vtable:
  .dword 0
  .dword 0
vtable_method:
  .dword hot
