// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-readelf -S -r %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt --use-old-text=0 -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s

// INPUT: .text
// INPUT-SAME: 000004
// INPUT:      Relocation section '.rela.text'
// INPUT:      R_LARCH_B26

// BOLT-NOT: failed to extract relocated value
// BOLT: BOLT-WARNING: Failed to analyze 1 relocations

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

/// GNU ld can leave relaxation-generated relocations just past the final byte of
/// .text. BOLT should ignore those relocation records instead of reading past
/// section contents.
  .reloc _start + 4, R_LARCH_B26, _start
