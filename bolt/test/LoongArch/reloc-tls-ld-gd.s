// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:       pcalau12i $a0, %pc_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  addi.d $a0, $a0, %pc_lo12(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  pcalau12i $a1, %pc_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  addi.d $a1, $a1, %pc_lo12(__BOLT_got_zero{{.*}})

  .text
  .globl _start
  .p2align 2
_start:
  la.tls.ld $a0, tdata
  la.tls.gd $a1, tdata
  ret
  .size _start, .-_start

  .section .tbss,"awT",@nobits
  .type tdata,@object
  .globl tdata
  .p2align 3
tdata:
  .quad 0
  .size tdata, .-tdata

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     pcalau12i $a0,
// OBJDUMP-NEXT:     addi.d $a0, $a0,
// OBJDUMP-NEXT:     pcalau12i $a1,
// OBJDUMP-NEXT:     addi.d $a1, $a1,
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
