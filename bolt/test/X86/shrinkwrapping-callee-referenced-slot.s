## This checks that unresolved call targets default to ArgAccesses::AssumeEverything
## and preserves all save slots conservatively.

# REQUIRES: system-linux

# RUN: llvm-mc -filetype=obj -triple x86_64-unknown-unknown %s -o %t.o
# RUN: link_fdata %s %t.o %t.fdata
# RUN: llvm-strip --strip-unneeded %t.o
# RUN: ld.lld %t.o -o %t.exe -q
# RUN: llvm-bolt %t.exe -relocs -o %t.out -data %t.fdata \
# RUN:   -frame-opt=all -experimental-shrink-wrapping \
# RUN:   -eliminate-unreachable=false 2>&1 | FileCheck %s --check-prefix=SW
# RUN: llvm-objdump -d --no-show-raw-insn %t.out | \
# RUN:   FileCheck %s --check-prefix=PROLOGUE

# SW: BOLT-INFO: Shrink wrapping moved 0 spills inserting load/stores and 0 spills inserting push/pops

## Verify the callee-saved spill is *not* moved, i.e. the prologue of _start still
## begins with the original `pushq %rbx`.
# PROLOGUE-LABEL: <_start>:
# PROLOGUE-NEXT:       pushq %rbx

  .text
  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
# FDATA: 0 [unknown] 0 1 _start 0 0 100
  pushq %rbx
  .cfi_def_cfa_offset 16
  .cfi_offset 3, -16
  subq $0x10, %rsp
  .cfi_def_cfa_offset 32
  leaq sink(%rip), %r11
  je cold
hot:
## indirect call here
  callq *%r11
  addq $0x10, %rsp
  .cfi_def_cfa_offset 16
  popq %rbx
  .cfi_def_cfa_offset 8
  ret
cold:
## uses %rbx, so the save must stay
  movl $0, %ebx
  addq $0x10, %rsp
  .cfi_def_cfa_offset 16
  popq %rbx
  .cfi_def_cfa_offset 8
  ret
  .cfi_endproc
  .size _start, .-_start

  .globl sink
  .type sink, %function
sink:
  retq
  .size sink, .-sink