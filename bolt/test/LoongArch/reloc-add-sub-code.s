// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-readelf -r %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt -o %t.bolt %t 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// INPUT:      R_LARCH_ADD32          {{.*}} code32_target
// INPUT-NEXT: R_LARCH_SUB32          {{.*}} code32_base
// INPUT-NEXT: R_LARCH_ADD64          {{.*}} code64_target
// INPUT-NEXT: R_LARCH_SUB64          {{.*}} code64_base

// BOLT-NOT: Unexpected LoongArch relocation type in code
// BOLT-NOT: failed to analyze

// OBJDUMP:      <_start>:
// OBJDUMP:          b
// OBJDUMP:          ret

  .text
  .globl _start
  .p2align 2
_start:
  // Force BOLT into relocation mode.
  .reloc ., R_LARCH_NONE
  b code_end

code32_base:
  .reloc ., R_LARCH_ADD32, code32_target
  .reloc ., R_LARCH_SUB32, code32_base
  .4byte code32_target - code32_base
code32_target:
  nop

code64_base:
  .reloc ., R_LARCH_ADD64, code64_target
  .reloc ., R_LARCH_SUB64, code64_base
  .8byte code64_target - code64_base
code64_target:
  nop

code_end:
  ret
  .size _start, .-_start
