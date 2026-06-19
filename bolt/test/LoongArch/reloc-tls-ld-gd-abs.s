// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK-NOT: BOLT-WARNING:
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:      lu12i.w $a0, %abs_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT: lu12i.w $a1, %abs_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT: ret

    .text
    .globl _start
    .p2align 2
_start:
    lu12i.w $a0, %ld_hi20(tdata)
    lu12i.w $a1, %gd_hi20(tdata)
    ret
    .size _start, .-_start

    .section .tbss,"awT",@nobits
    .globl tdata
    .p2align 3
tdata:
    .quad 0
    .size tdata, .-tdata

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     lu12i.w $a0,
// OBJDUMP-NEXT:     lu12i.w $a1,
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
