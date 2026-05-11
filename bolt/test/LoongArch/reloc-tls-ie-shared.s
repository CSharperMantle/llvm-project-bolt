// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --shared --emit-relocs -o %t.so %t.o
// RUN: llvm-bolt --print-cfg --print-only=tls_ie -o %t.null %t.so | FileCheck %s

// CHECK-LABEL: Binary Function "tls_ie" after building cfg {
// CHECK-LABEL: .LBB00
// CHECK:      pcalau12i $a0, %pc_hi20(__BOLT_got_zero+{{[0-9]+}})
// CHECK-NEXT: ld.d $a0, $a0, %pc_lo12(__BOLT_got_zero+{{[0-9]+}})
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
