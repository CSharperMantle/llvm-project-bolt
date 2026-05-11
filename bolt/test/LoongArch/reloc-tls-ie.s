// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=tls_ie -o %t.null %t | FileCheck %s

// CHECK-LABEL: Binary Function "tls_ie" after building cfg {
// CHECK-LABEL: .LBB00
// CHECK:      nop # NOP: 1
// CHECK-NEXT: ori $a0, $zero, 0
    .text
    .globl tls_ie, _start
    .p2align 2
tls_ie:
_start:
    la.tls.ie $a0, i
    ret
    .size tls_ie, .-tls_ie

    .section .tbss,"awT",@nobits
    .type i,@object
    .globl i
    .p2align 3
i:
    .quad 0
    .size i, .-i
