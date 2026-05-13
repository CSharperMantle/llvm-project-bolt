// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s
// RUN: llvm-readelf -rW %t.bolt | FileCheck --check-prefix=RELOC %s

// CHECK-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:       lu12i.w $a0, %abs_hi20(data_sym)
// CHECK-NEXT:  ori $a0, $a0, %abs_lo12(data_sym)
// CHECK-NEXT:  lu32i.d $a0, %abs64_lo20(data_sym)
// CHECK-NEXT:  lu52i.d $a0, $a0, %abs64_hi12(data_sym)

  .text
  .globl _start
  .p2align 2
_start:
  lu12i.w $a0, %abs_hi20(data_sym)
  ori $a0, $a0, %abs_lo12(data_sym)
  lu32i.d $a0, %abs64_lo20(data_sym)
  lu52i.d $a0, $a0, %abs64_hi12(data_sym)
  ret
  .size _start, .-_start

  .data
  .globl data_sym
  .p2align 3
data_sym:
  .quad 0

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     lu12i.w $a0,
// OBJDUMP-NEXT:     ori $a0, $a0,
// OBJDUMP-NEXT:     lu32i.d $a0,
// OBJDUMP-NEXT:     lu52i.d $a0, $a0,
// OBJDUMP-NEXT:     ret

// RELOC: There are no relocations in this file.
