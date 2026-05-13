// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:       pcaddu18i $ra, %call36(f)
// CHECK-NEXT:  jirl $ra, $ra, {{.+}}

  .text
  .globl _start
  .p2align 2
_start:
  pcaddu18i $ra, %call36(f)
  jirl $ra, $ra, 0
  ret
  .size _start, .-_start

  .globl f
  .p2align 2
f:
  ret
  .size f, .-f

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     pcaddu18i $ra,
// OBJDUMP-NEXT:     jirl $ra, $ra,
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
