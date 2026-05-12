// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:       pcaddu12i $a0, %pcadd_hi20(data_sym)
// CHECK-NEXT:  addi.d $a0, $a0, %pcadd_lo12(_start)
// CHECK:       pcaddu12i $a1, %pcadd_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  ld.d $a1, $a1, %pcadd_lo12(.Ltmp0

  .text
  .globl _start
  .p2align 2
_start:
.Lpcadd_hi0:
  pcaddu12i $a0, %pcadd_hi20(data_sym)
  addi.d $a0, $a0, %pcadd_lo12(.Lpcadd_hi0)
.Lpcadd_hi1:
  pcaddu12i $a1, %got_pcadd_hi20(data_sym)
  ld.d $a1, $a1, %got_pcadd_lo12(.Lpcadd_hi1)
  ret
  .size _start, .-_start

  .data
  .globl data_sym
  .p2align 3
data_sym:
  .quad 0

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     pcaddu12i $a0,
// OBJDUMP-NEXT:     addi.d $a0, $a0,
// OBJDUMP-NEXT:     pcaddu12i $a1,
// OBJDUMP-NEXT:     ld.d $a1, $a1,
// OBJDUMP-NEXT:     ret
