// RUN: llvm-mc --triple=loongarch64 --filetype=obj --mattr=+relax -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-objdump -dr --no-show-raw-insn %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// INPUT:      <_start>:
// INPUT-NEXT:     pcaddi $a0, 0
// INPUT-NEXT:     R_LARCH_RELAX _start
// INPUT-NEXT:     R_LARCH_RELAX *ABS*
// INPUT-NEXT:     R_LARCH_PCREL20_S2 _start

// BOLT-NOT: BOLT-WARNING: Failed to analyze
// BOLT-LABEL: Binary Function "_start" after building cfg {
// BOLT:       IsSimple    : 1
// BOLT:       pcaddi $a0, %pcrel_20(_start)
// BOLT-NEXT:  ret

// OBJDUMP:      0000000000400000 <_start>:
// OBJDUMP-NEXT:     pcaddi $a0, 0
// OBJDUMP-NEXT:     ret

  .text
  .globl _start
  .p2align 2
_start:
  la.pcrel $a0, _start
  ret
  .size _start, .-_start
