// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: link_fdata %s %t.o %t.fdata
// RUN: llvm-strip --strip-unneeded %t.o
// RUN: ld.lld --emit-relocs -e dummy -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --relocs --data=%t.fdata --frame-opt=all \
// RUN:   --experimental-shrink-wrapping --reorder-blocks=none --print-fop \
// RUN:   --print-only=_start --debug-only=reaching-def-or-use 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck %s --check-prefix=OBJDUMP

// CHECK: RegReachingDefs classes for "_start": 7 tracked occurrences, 4 classes
// CHECK: RegReachingUses classes for "_start": 11 tracked occurrences, 7 classes
// CHECK: BOLT-INFO: Shrink wrapping moved 1 spills inserting load/stores and 0 spills inserting push/pops

// CHECK-LABEL: Binary Function "_start" after frame-optimizer
// CHECK:       .LFT0 (4 instructions, align : 1)
// CHECK:         st.d $s0, $sp, 0
// CHECK-NEXT:    !CFI
// CHECK-NEXT:    addi.d $s0, $a1, 1
// CHECK-NEXT:    beq $s0, $a2, .LSplitEdge0
// CHECK-NEXT:  Successors: .LSplitEdge0 {{.*}}, .LSplitEdge1
// CHECK:       .LSplitEdge1
// CHECK:         ld.d $s0, $sp, 0
// CHECK-NEXT:    b .Ltmp3
// CHECK:       .LSplitEdge0
// CHECK:         ld.d $s0, $sp, 0
// CHECK-NEXT:    !CFI
// CHECK-NEXT:    !CFI
// CHECK-NEXT:    b .Ltmp1

// OBJDUMP-LABEL: <_start>:
// OBJDUMP:         addi.d $s0, $a1, 1
// OBJDUMP-NEXT:    beq $s0, $a2,
// OBJDUMP-NEXT:    ld.d $s0, $sp, 0
// OBJDUMP-NEXT:    b
// OBJDUMP-NEXT:    ld.d $s0, $sp, 0
// OBJDUMP-NEXT:    b

  .text
  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
// FDATA: 0 [unknown] 0 1 _start 0 0 1000
  addi.d $sp, $sp, -16
  .cfi_def_cfa_offset 16
  st.d $s0, $sp, 0
  .cfi_offset 23, -16
entry_branch:
  beqz $a0, hot_dispatch
// FDATA: 1 _start #entry_branch# 1 _start #hot_dispatch# 0 999
cold:
  addi.d $s0, $a1, 1
cold_branch:
  beq $s0, $a2, exit_a
  .cfi_def_cfa_offset 16
// FDATA: 1 _start #cold_branch# 1 _start #exit_a# 0 1
exit_b:
  addi.d $a0, $a0, 2
  b epilogue
hot_dispatch:
  beqz $a3, exit_a
  b exit_b
exit_a:
  addi.d $a0, $a0, 1
epilogue:
  ld.d $s0, $sp, 0
  .cfi_restore 23
  addi.d $sp, $sp, 16
  .cfi_def_cfa_offset 0
  ret
  .cfi_endproc
  .size _start, .-_start

/// Create a relocation against code so BOLT can move the enlarged function.
  .globl dummy
  .type dummy, %function
dummy:
  bl _start
  ret
  .size dummy, .-dummy
