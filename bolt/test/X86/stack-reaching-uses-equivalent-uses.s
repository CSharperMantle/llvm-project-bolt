## Check that StackReachingUses coalesces frame loads and call argument uses
## with identical observable metadata while keeping different registers and
## access sizes in separate classes.

# REQUIRES: system-linux

# RUN: llvm-mc -filetype=obj -triple x86_64-unknown-unknown %s -o %t.o
# RUN: llvm-strip --strip-unneeded %t.o
# RUN: ld.lld %t.o -o %t.exe -q
# RUN: llvm-bolt %t.exe -o %t.out --relocs --frame-opt=all \
# RUN:   --frame-opt-rm-stores --debug-only=sru 2>&1 | FileCheck %s

# CHECK: StackReachingUses classes for "_start": 6 tracked occurrences, 4 classes

  .text
  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
  cmpq $0, %rdi
  je .Lsame_a
  cmpq $1, %rdi
  je .Lsame_b
  cmpq $2, %rdi
  je .Ldifferent_reg

.Ldifferent_size:
  movl -8(%rsp), %eax
  ud2

.Lsame_a:
  movq -8(%rsp), %rax
  callq callee
  ud2

.Lsame_b:
  movq -8(%rsp), %rax
  callq callee
  ud2

.Ldifferent_reg:
  movq -8(%rsp), %rcx
  ud2
  .cfi_endproc
  .size _start, .-_start

  .globl callee
  .type callee, %function
callee:
  .cfi_startproc
  movq 8(%rsp), %rax
  retq
  .cfi_endproc
  .size callee, .-callee
