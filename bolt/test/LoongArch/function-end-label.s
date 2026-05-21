/// Test that a zero-sized local symbol at function end is not treated as a
/// primary function symbol during symbol table rewriting.
// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt -o %t.bolt %t
// RUN: llvm-nm -n %t.bolt | FileCheck --check-prefix=NM %s

// NM: _start
// NM: start_end
// NM: next

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

start_end:
  nop

  .globl next
  .p2align 2
next:
  ret
  .size next, .-next
