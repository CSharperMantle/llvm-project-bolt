/// Check LoongArch indirect-call promotion for normal calls and tail calls.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: link_fdata %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --icp=calls \
// RUN:   --icp-calls-topn=1 --print-icp -v=1 2>&1 | FileCheck %s

// CHECK-DAG: BOLT-INFO: ICP succeeded in caller
// CHECK-DAG: BOLT-INFO: ICP succeeded in tail_caller

// CHECK-LABEL: Binary Function "caller" after indirect-call-promotion
// CHECK:      pcalau12i $t8, %pc_hi20(hot)
// CHECK-NEXT: addi.d $t8, $t8, %pc_lo12(hot)
// CHECK-NEXT: bne $a0, $t8,
// CHECK:      bl hot
// CHECK:      jirl $ra, $a0, 0
// CHECK:      End of Function "caller"

// CHECK-LABEL: Binary Function "tail_caller" after indirect-call-promotion
// CHECK:      pcalau12i $t8, %pc_hi20(hot)
// CHECK-NEXT: addi.d $t8, $t8, %pc_lo12(hot)
// CHECK-NEXT: bne $a0, $t8,
// CHECK:      b hot # TAILCALL
// CHECK:      jr $a0 # TAILCALL
// CHECK:      End of Function "tail_caller"

  .text
  .globl _start
  .p2align 2
_start:
  bl caller
  bl tail_caller
  ret
  .size _start, .-_start

  .globl caller
  .p2align 2
caller:
  pcalau12i $a0, %pc_hi20(funcs)
  ld.d $a0, $a0, %pc_lo12(funcs)
Lcall:
  jirl $ra, $a0, 0
// FDATA: 1 caller #Lcall# 1 hot 0 0 100
  ret
  .size caller, .-caller

  .globl tail_caller
  .p2align 2
tail_caller:
  pcalau12i $a0, %pc_hi20(hot)
  addi.d $a0, $a0, %pc_lo12(hot)
  jirl $zero, $a0, 0
// FDATA: 1 tail_caller 8 1 hot 0 0 100
  .size tail_caller, .-tail_caller

  .globl hot
  .p2align 2
hot:
  ret
  .size hot, .-hot

  .data
  .globl funcs
funcs:
  .dword hot
