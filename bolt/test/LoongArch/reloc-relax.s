// RUN: llvm-mc --triple=loongarch64 --filetype=obj --mattr=+relax -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-objdump -dr --no-show-raw-insn %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt --use-old-text=0 --print-loongarch-relaxation --print-only=_start -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// INPUT:      <_start>:
// INPUT-NEXT:     pcaddi $a0,
// INPUT-NEXT:     R_LARCH_RELAX data_sym
// INPUT-NEXT:     R_LARCH_RELAX *ABS*
// INPUT-NEXT:     R_LARCH_PCREL20_S2 data_sym

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// BOLT-LABEL: Binary Function "_start" after loongarch-relaxation {
// BOLT:       pcalau12i $a0, %pc_hi20(data_sym)
// BOLT-NEXT:  addi.d $a0, $a0, %pc_lo12(data_sym)

  .text
  .globl _start
  .p2align 2
_start:
  la.pcrel $a0, data_sym
  ret
  .size _start, .-_start

  .data
  .globl data_sym
  .p2align 3
data_sym:
  .quad 0

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     pcalau12i $a0,
// OBJDUMP-NEXT:     addi.d $a0, $a0,
// OBJDUMP-NEXT:     ret
