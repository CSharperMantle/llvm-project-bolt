/// Port of X86/indirect-goto.test for LoongArch.
/// Check that strict mode conservatively connects an always-unknown indirect
/// jump to every internally referenced destination.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --relocs --strict --print-cfg --print-only=strict_unknown \
// RUN:   -o %t.bolt %t 2>&1 | FileCheck %s

// CHECK-LABEL: Binary Function "strict_unknown" after building cfg
// CHECK: IsSimple    : 1
// CHECK: Unknown CF  : true
// CHECK: jr $a1 # UNKNOWN CONTROL FLOW
// CHECK-NEXT: Successors: .Ltmp0, .Ltmp1, .Ltmp2

  .text
  .reloc 0, R_LARCH_NONE
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

  .globl strict_unknown
  .p2align 2
strict_unknown:
  beqz $a0, .Ldispatch
  ori   $a0, $a0, 1
.Ldispatch:
  jr    $a1
.Ldest0:
  ret
.Ldest1:
  ret
.Ldest2:
  ret
  .size strict_unknown, .-strict_unknown

  .section .rodata,"a",@progbits
  .p2align 3
.Ltargets:
  .dword .Ldest0
  .dword .Ldest1
  .dword .Ldest2
