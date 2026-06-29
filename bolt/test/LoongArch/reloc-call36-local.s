// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck --check-prefix=OBJDUMP-ORIG %s
// RUN: llvm-bolt --debug-only=bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP-BOLT %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK-NOT:   BOLT-WARNING: Failed to analyze
/// Both %call36 sequences should be discovered.
// CHECK:       BOLT-DEBUG: relocation-less LoongArch call pair found at
// CHECK-NEXT:  BOLT-DEBUG: relocation-less LoongArch call pair found at

// CHECK-LABEL: Binary Function "_start" after building cfg {
// CHECK:       pcaddu18i $ra, %call36("f/1")
// CHECK-NEXT:  jirl $ra, $ra,

  .text
  .globl _start
  .p2align 2
_start:
  pcaddu18i $ra, %call36(f)
  jirl $ra, $ra, 0
  li.d $a1, 0xdeadbeefcafebabe
  li.d $a7, 93
  pcaddu18i $t0, %call36(g)
  jirl $zero, $t0, 0
/// nop-fill to make pcaddu18i non-zero
  .rept 0x8000
  nop
  .endr
  syscall 0
  .size _start, .-_start

  .type f, @function
  .p2align 2
f:
  li.d $a0, 0xdeadbeefcafebabe
  ret
  .size f, .-f

  .type g, @function
  .p2align 2
g:
  sub.d $a0, $a0, $a1
  sltu $a0, $zero, $a0
  li.d $a7, 93
  syscall 0
  .size g, .-g

// OBJDUMP-ORIG:      <_start>:
// OBJDUMP-ORIG-NEXT:   pcaddu18i $ra, 1
// OBJDUMP-ORIG-NEXT:   jirl $ra, $ra,

// OBJDUMP-BOLT:      <_start>:
// OBJDUMP-BOLT-NEXT:   pcaddu18i $ra, 1
// OBJDUMP-BOLT-NEXT:   jirl $ra, $ra,

// RELOC: There are no relocations in this file.
