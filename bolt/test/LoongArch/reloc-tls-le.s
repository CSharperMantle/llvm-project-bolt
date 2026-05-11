// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=tls_le -o %t.null %t | FileCheck %s
// RUN: llvm-objdump -d %t.null | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.null | FileCheck --check-prefix=RELOC %s

// CHECK-LABEL: Binary Function "tls_le{{.*}}" after building cfg {
// CHECK-LABEL: .LBB00
// CHECK:      lu12i.w $a0, 0
// CHECK-NEXT: ori $a0, $a0, 0
// OBJDUMP:      0000000000400000 <tls_le>:
// OBJDUMP-NEXT:     lu12i.w $a0, 0
// OBJDUMP-NEXT:     ori $a0, $a0, 0
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
    .text
    .globl tls_le, _start
    .p2align 2
tls_le:
_start:
    lu12i.w $a0, %le_hi20(i)
    ori     $a0, $a0, %le_lo12(i)
    ret
    .size _start, .-_start

    .section .tbss,"awT",@nobits
    .type i,@object
    .globl i
    .p2align 3
i:
    .quad 0
    .size i, .-i
