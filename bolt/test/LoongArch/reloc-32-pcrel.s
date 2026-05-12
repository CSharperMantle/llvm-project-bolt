// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-readelf -x .data %t.bolt | FileCheck --check-prefix=DATA %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// BOLT-NOT: BOLT-WARNING: Failed to analyze

  .text
  .globl _start
  .p2align 2
_start:
  nop
  ret
  .size _start, .-_start

  .data
  .globl delta
  .p2align 2
delta:
// DATA:      Hex dump of section '.data':
// DATA-NEXT: 0x{{.*}} f8fffeff
  .reloc ., R_LARCH_32_PCREL, _start
  .4byte 0

// RELOC: There are no relocations in this file.
