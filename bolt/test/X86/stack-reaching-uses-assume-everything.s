## Check that an AssumeEverything argument-use class prevents removal of a
## preceding frame store.

# REQUIRES: system-linux

# RUN: llvm-mc -filetype=obj -triple x86_64-unknown-unknown %s -o %t.o
# RUN: llvm-strip --strip-unneeded %t.o
# RUN: ld.lld %t.o -o %t.exe -q
# RUN: llvm-bolt %t.exe -o %t.out --relocs --frame-opt=all \
# RUN:   --frame-opt-rm-stores --debug-only=sru 2>&1 | FileCheck %s
# RUN: llvm-objdump -d %t.out | FileCheck %s --check-prefix=OBJDUMP

# CHECK: StackReachingUses classes for "_start": 1 tracked occurrences, 1 classes
# CHECK: BOLT-INFO: FOP optimized 0 redundant load(s) and 0 unused store(s)

# OBJDUMP-LABEL: <_start>:
# OBJDUMP: movq{{.*}}%rax, (%rsp)

  .text
  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
  leaq sink(%rip), %r10
  subq $16, %rsp
  .cfi_def_cfa_offset 24
  movq %rax, (%rsp)
  callq *%r11
  ud2
  .cfi_endproc
  .size _start, .-_start

  .globl sink
  .type sink, %function
sink:
  retq
  .size sink, .-sink
