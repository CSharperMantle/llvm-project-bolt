// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-readelf -x .data %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-readelf -x .data %t.bolt | FileCheck --check-prefix=DATA %s

// INPUT:      Hex dump of section '.data':
// INPUT-NEXT: 0x{{.*}} 08080008

// BOLT-NOT: BOLT-WARNING: Failed to analyze

  .text
  .globl _start
  .p2align 2
_start:
  ret
middle:
  ret
end:
  ret
  .size _start, .-_start

  .data
  .globl values
values:
  .reloc ., R_LARCH_ADD8, end
  .reloc ., R_LARCH_SUB8, _start
  .byte 0
  .reloc ., R_LARCH_ADD16, end
  .reloc ., R_LARCH_SUB16, _start
  .2byte 0
  .reloc ., R_LARCH_ADD_ULEB128, end
  .reloc ., R_LARCH_SUB_ULEB128, _start
  .uleb128 0

// DATA:      Hex dump of section '.data':
// DATA-NEXT: 0x{{.*}} 08080008
