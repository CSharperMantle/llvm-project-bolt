/// Check that StackReachingUses coalesces frame loads and call argument uses
/// with identical observable metadata while keeping different registers and
/// access sizes in separate classes.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: llvm-strip --strip-unneeded %t.o
// RUN: ld.lld --emit-relocs -e _start -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --relocs --frame-opt=all \
// RUN:   --frame-opt-rm-stores --debug-only=sru 2>&1 | FileCheck %s

// CHECK: StackReachingUses classes for "_start": 6 tracked occurrences, 4 classes

  .text
  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
  beqz $a2, .Lsame_a
  addi.d $t1, $zero, 1
  beq $a2, $t1, .Lsame_b
  addi.d $t1, $zero, 2
  beq $a2, $t1, .Ldifferent_reg

.Ldifferent_size:
  ld.w $a0, $sp, -8
  break 0

.Lsame_a:
  ld.d $a0, $sp, -8
  bl callee
  break 0

.Lsame_b:
  ld.d $a0, $sp, -8
  bl callee
  break 0

.Ldifferent_reg:
  ld.d $a1, $sp, -8
  break 0
  .cfi_endproc
  .size _start, .-_start

  .globl callee
  .type callee, %function
callee:
  .cfi_startproc
  ld.d $a0, $sp, 0
  ret
  .cfi_endproc
  .size callee, .-callee
