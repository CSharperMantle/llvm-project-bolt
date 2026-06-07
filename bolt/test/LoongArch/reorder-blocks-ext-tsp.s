// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: echo "1 _start 0 1 _start c 0 1000" > %t.fdata
// RUN: llvm-bolt --reorder-blocks=ext-tsp --data=%t.fdata -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// CHECK-NOT: BOLT-WARNING:

// CHECK: basic block reordering modified layout of 1 functions
// CHECK: 1 out of 1 functions were overwritten

// OBJDUMP:      {{.*}} <_start>:
// OBJDUMP-NEXT:    {{(bne|beq)}} $t0, $t1, {{.*}}
// OBJDUMP-NEXT:    addi.d $a0, $zero, 2
// OBJDUMP-NEXT:    b {{.*}}
// OBJDUMP-NEXT:    addi.d $a0, $zero, 1
// OBJDUMP-NEXT:    ret

  .text
  .globl _start
  .p2align 2
_start:
  beq $t0, $t1, .L_taken
  addi.d $a0, $zero, 1
  b .L_done
.L_taken:
  addi.d $a0, $zero, 2
.L_done:
  ret
  .size _start, .-_start
