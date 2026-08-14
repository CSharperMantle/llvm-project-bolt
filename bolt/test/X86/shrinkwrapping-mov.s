## This checks that shrink wrapping correctly drops moving push/pops when
## there is a MOV instruction loading the value of the stack pointer in
## order to do pointer arithmetic with a stack address.


# REQUIRES: system-linux

# RUN: llvm-mc -filetype=obj -triple x86_64-unknown-unknown \
# RUN:   %s -o %t.o
# RUN: link_fdata %s %t.o %t.fdata
# RUN: llvm-strip --strip-unneeded %t.o
# RUN: ld.lld %t.o -o %t.exe -q
# RUN: llvm-bolt %t.exe -relocs -o %t.out -data %t.fdata \
# RUN:     -frame-opt=all -simplify-conditional-tail-calls=false \
# RUN:     -experimental-shrink-wrapping \
# RUN:     -eliminate-unreachable=false \
# RUN:     -debug-only=reaching-def-or-use 2>&1 | FileCheck %s

  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
# FDATA: 0 [unknown] 0 1 _start 0 0 1
  push  %rbp
  mov   %rsp, %rbp
  push  %rbx
  push  %r14
  subq  $0x20, %rsp
  je  b
c:
  addq  $0x20, %rsp
  pop %r14
  pop %rbx
  pop %rbp
  ret
b:
  je  f
  jmp *JT(,%rdi,8)
d:
  mov %r14, %rdi
  mov %rbx, %rdi
  mov %rbp, %rdi
  sub 0x20, %rdi
f:
  addq  $0x20, %rsp
  pop %r14
  pop %rbx
  pop %rbp
  ret
  .cfi_endproc
  .size _start, .-_start
  .data
JT:
  .quad c
  .quad d
  .quad f


# CHECK: RegReachingDefs classes for "_start": 17 tracked occurrences, 8 classes
# CHECK: RegReachingUses classes for "_start": 20 tracked occurrences, 10 classes
# CHECK: BOLT-INFO: Shrink wrapping moved 2 spills inserting load/stores and 0 spills inserting push/pops
