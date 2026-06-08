// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-objdump -dr --no-show-raw-insn %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// INPUT:      <_start>:
// INPUT:          R_LARCH_ALIGN *ABS*+0x8

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// BOLT-LABEL: Binary Function "_start{{.*}}" after building cfg {
// BOLT:       IsSimple    : 1

  .text
  .globl _start
  .p2align 2
_start:
  nop
  .reloc ., R_LARCH_ALIGN, 8
  nop
  nop
  ret
  .size _start, .-_start

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     ret
