// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck --check-prefix=OBJDUMP-ORIG %s
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP-BOLT %s
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
/// nop-fill to make pcaddu18i non-zero
  .rept 0x8000
  nop
  .endr
  li.d $t0, 0xdeadbeefcafebabe
  sub.d $a0, $a0, $t0
  sltu $a0, $zero, $a0
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .globl f
  .p2align 2
f:
  li.d $a0, 0xdeadbeefcafebabe
  ret
  .size f, .-f

// OBJDUMP-ORIG:      <_start>:
// OBJDUMP-ORIG-NEXT:   pcaddu18i $ra, 1
// OBJDUMP-ORIG-NEXT:   jirl $ra, $ra,

/// BOLT should remove the many nops in between.
// OBJDUMP-BOLT:      <_start>:
// OBJDUMP-BOLT-NEXT:   pcaddu18i $ra, 0
// OBJDUMP-BOLT-NEXT:   jirl $ra, $ra,

// RELOC: There are no relocations in this file.
