// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --simplify-rodata-loads=1 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT: BOLT-INFO: simplified 5 out of 5 loads from a statically computed address.

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT: ori $t0, $zero, 0
/// Case 1
// OBJDUMP-NEXT: pcalau12i $a0, {{-?[0-9]+}}
// OBJDUMP-NEXT: addi.d $a0, $zero, 1234
// OBJDUMP-NEXT: add.d $t0, $t0, $a0
/// Case 2
// OBJDUMP-NEXT: pcalau12i $a0, {{-?[0-9]+}}
// OBJDUMP-NEXT: ori $a0, $zero, 2048
// OBJDUMP-NEXT: add.d $t0, $t0, $a0
/// Case 3
// OBJDUMP-NEXT: pcalau12i $a0, {{-?[0-9]+}}
// OBJDUMP-NEXT: addi.d $a0, $zero, 56
// OBJDUMP-NEXT: add.d $t0, $t0, $a0
/// Case 4
// OBJDUMP-NEXT: pcalau12i $a0, {{-?[0-9]+}}
// OBJDUMP-NEXT: addi.d $a0, $zero, 16
// OBJDUMP-NEXT: add.d $t0, $t0, $a0
/// Case 5
// OBJDUMP-NEXT: pcalau12i $a0, {{-?[0-9]+}}
// OBJDUMP-NEXT: lu12i.w $a0, -217109
// OBJDUMP-NEXT: lu32i.d $a0, 397876
// OBJDUMP-NEXT: lu52i.d $a0, $a0, -1657
// OBJDUMP-NEXT: ori $a0, $a0, 2750
// OBJDUMP-NEXT: add.d $t0, $t0, $a0

  .text
  .globl _start
  .p2align 2
_start:
  li.d $t0, 0
  // Case 1
  // ld.w, value 1234 fits isInt<12> -> addi.d rd, $zero, 1234
  pcalau12i $a0, %pc_hi20(.Lw1)
  ld.w $a0, $a0, %pc_lo12(.Lw1)
  add.d $t0, $t0, $a0

  // Case 2
  // ld.wu, value 0x800 (2048) fits isUInt<12> -> ori rd, $zero, 2048
  pcalau12i $a0, %pc_hi20(.Lw2)
  ld.wu $a0, $a0, %pc_lo12(.Lw2)
  add.d $t0, $t0, $a0

  // Case 3
  // ld.bu, value 56 fits isInt<12> -> addi.d rd, $zero, 56
  pcalau12i $a0, %pc_hi20(.Lb)
  ld.bu $a0, $a0, %pc_lo12(.Lb)
  add.d $t0, $t0, $a0

  // Case 4
  // ld.h, value 16 fits isInt<12> -> addi.d rd, $zero, 16
  pcalau12i $a0, %pc_hi20(.Lh)
  ld.h $a0, $a0, %pc_lo12(.Lh)
  add.d $t0, $t0, $a0

  // Case 5
  // LD_D, value 0x98761234cafebabe -> createLoadImmediate
  pcalau12i $a0, %pc_hi20(.Ld)
  ld.d $a0, $a0, %pc_lo12(.Ld)
  add.d $t0, $t0, $a0

  // exit(0)
  li.d $t1, (1234+2048+56+16+0x98761234cafebabe)
  sub.d $a0, $t0, $t1
  sltu $a0, $zero, $a0
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .section .rodata
  .p2align 2
.Lw1:
  .word 1234
.Lw2:
  .word 0x800
.Lb:
  .byte 56
  .p2align 1
.Lh:
  .2byte 16
  .p2align 3
.Ld:
  .8byte 0x98761234cafebabe
