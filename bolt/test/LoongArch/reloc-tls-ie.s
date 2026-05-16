// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=tls_ie -o %t.null %t | FileCheck %s
// RUN: llvm-objdump -d %t.null | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.null | FileCheck --check-prefix=RELOC %s

// CHECK-LABEL: Binary Function "tls_ie" after building cfg {
// CHECK-LABEL: .LBB00
// CHECK:      nop # NOP: 1
// CHECK-NEXT: ori $a0, $zero, %pc_lo12(__BOLT_got_zero)
// OBJDUMP:      0000000000400000 <tls_ie>:
// OBJDUMP-NEXT:     nop
// OBJDUMP-NEXT:     ori $a0, $zero, 0
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
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
