// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:       pcaddi $a0, %pcrel_20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  pcaddi $a1, %pcrel_20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  nop # NOP: 1

  .text
  .globl _start
  .p2align 2
_start:
  pcaddi $a0, %ld_pcrel_20(tdata)
  pcaddi $a1, %gd_pcrel_20(tdata)
  pcaddi $a2, %desc_pcrel_20(tdata)
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
// OBJDUMP-NEXT:     nop
// OBJDUMP-NEXT:     ret
