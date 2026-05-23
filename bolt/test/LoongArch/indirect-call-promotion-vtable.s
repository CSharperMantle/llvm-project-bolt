/// Check LoongArch indirect-call promotion with vtable load elimination.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs -o %t %t.o
// RUN: link_fdata %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --icp=calls \
// RUN:   --icp-calls-topn=1 --icp-eliminate-loads --print-icp -v=1 \
// RUN:   2>&1 | FileCheck %s

// CHECK-DAG: BOLT-INFO: ICP found virtual method call in caller at
// CHECK-DAG: BOLT-INFO: ICP succeeded in caller @
// CHECK-DAG: BOLT-INFO: ICP found virtual method call in caller_ldptr at
// CHECK-DAG: BOLT-INFO: ICP succeeded in caller_ldptr @

// CHECK-LABEL: Binary Function "caller" after indirect-call-promotion
// CHECK:      ld.d $a0, $a0, 0
// CHECK-NEXT: pcalau12i $t8, %pc_hi20(vtable)
// CHECK-NEXT: addi.d $t8, $t8, %pc_lo12(vtable)
// CHECK-NEXT: bne $a0, $t8,
// CHECK:      bl hot
// CHECK:      ld.d $a1, $a0, 16
// CHECK-NEXT: jirl $ra, $a1, 0
// CHECK:      End of Function "caller"

// CHECK-LABEL: Binary Function "caller_ldptr" after indirect-call-promotion
// CHECK:      ldptr.d $a0, $a0, 0
// CHECK-NEXT: pcalau12i $t8, %pc_hi20(vtable)
// CHECK-NEXT: addi.d $t8, $t8, %pc_lo12(vtable)
// CHECK-NEXT: bne $a0, $t8,
// CHECK:      bl hot
// CHECK:      ldptr.d $a1, $a0, 16
// CHECK-NEXT: jirl $ra, $a1, 0
// CHECK:      End of Function "caller_ldptr"

  .text
  .globl _start
  .p2align 2
_start:
  bl caller
  bl caller_ldptr
  ret
  .size _start, .-_start

  .globl caller
  .p2align 2
caller:
  pcalau12i $a0, %pc_hi20(obj)
  ld.d $a0, $a0, %pc_lo12(obj)
  ld.d $a0, $a0, 0
  ld.d $a1, $a0, 16
  jirl $ra, $a1, 0
// FDATA: 1 caller 10 1 hot 0 0 100
  ret
  .size caller, .-caller

  .globl caller_ldptr
  .p2align 2
caller_ldptr:
  pcalau12i $a0, %pc_hi20(obj)
  ld.d $a0, $a0, %pc_lo12(obj)
  ldptr.d $a0, $a0, 0
  ldptr.d $a1, $a0, 16
  jirl $ra, $a1, 0
// FDATA: 1 caller_ldptr 10 1 hot 0 0 100
  ret
  .size caller_ldptr, .-caller_ldptr

// FDATA: 4 caller c 4 vtable #vtable_method# 100
// FDATA: 4 caller_ldptr c 4 vtable #vtable_method# 100

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
