// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=tls_ie_abs -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK-NOT: BOLT-WARNING:
// CHECK-LABEL: Binary Function "tls_ie_abs" after building cfg {
// CHECK:      lu12i.w $a0, %abs_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT: ori $a0, $a0, %abs_lo12(__BOLT_got_zero{{.*}})
// CHECK-NEXT: ret

    .text
    .globl tls_ie_abs, _start
    .p2align 2
tls_ie_abs:
_start:
    lu12i.w $a0, %ie_hi20(i)
    ori $a0, $a0, %ie_lo12(i)
    ret
    .size tls_ie_abs, .-tls_ie_abs

    .section .tbss,"awT",@nobits
    .globl i
    .p2align 3
i:
    .quad 0
    .size i, .-i

// OBJDUMP:      <tls_ie_abs>:
// OBJDUMP-NEXT:     lu12i.w $a0,
// OBJDUMP-NEXT:     ori $a0, $a0,
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
