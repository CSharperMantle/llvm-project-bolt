/// Check that vtable load elimination is disabled when an intervening call can
/// clobber the register holding the vtable address.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs -o %t %t.o
// RUN: link_fdata %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --icp=calls \
// RUN:   --icp-calls-topn=1 --icp-eliminate-loads --print-icp -v=1 \
// RUN:   2>&1 | FileCheck %s

// CHECK: BOLT-INFO: ICP succeeded in caller @
// CHECK-LABEL: Binary Function "caller" after indirect-call-promotion
// CHECK:      ldptr.d $s0, $t0, 16
// CHECK-NEXT: bl clobber
// CHECK-NEXT: pcalau12i $t8, %pc_hi20(hot)
// CHECK-NEXT: addi.d $t8, $t8, %pc_lo12(hot)
// CHECK-NEXT: bne $s0, $t8,
// CHECK:      bl hot
// CHECK:      jirl $ra, $s0, 0
// CHECK:      End of Function "caller"

  .text
  .globl _start
  .p2align 2
_start:
  bl caller
  ret
  .size _start, .-_start

  .globl caller
  .p2align 2
caller:
  addi.d $sp, $sp, -16
  st.d $ra, $sp, 8
  st.d $s0, $sp, 0
  pcalau12i $t0, %pc_hi20(vtable)
  addi.d $t0, $t0, %pc_lo12(vtable)
  ldptr.d $s0, $t0, 16
  bl clobber
  jirl $ra, $s0, 0
// FDATA: 1 caller 1c 1 hot 0 0 100
  ld.d $ra, $sp, 8
  ld.d $s0, $sp, 0
  addi.d $sp, $sp, 16
  ret
  .size caller, .-caller

// FDATA: 4 caller 14 4 vtable #vtable_method# 100

  .globl clobber
  .p2align 2
clobber:
  move $t0, $zero
  ret
  .size clobber, .-clobber

  .globl hot
  .p2align 2
hot:
  ret
  .size hot, .-hot

  .section .rodata,"a",@progbits
  .globl vtable
vtable:
  .dword 0
  .dword 0
vtable_method:
  .dword hot
