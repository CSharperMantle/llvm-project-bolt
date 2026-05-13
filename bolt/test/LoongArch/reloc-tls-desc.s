// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -shared --emit-relocs -o %t %t.o
// RUN: llvm-bolt --print-cfg --print-only=_start -o %t.bolt %t 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// CHECK-NOT: BOLT-WARNING: Failed to analyze
// CHECK-LABEL: Binary Function "_start{{.*}}" after building cfg {
// CHECK:       pcalau12i $a0, %pc_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  addi.d $a0, $a0, %pc_lo12(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  ld.d $ra, $a0, 0
// CHECK-NEXT:  jirl $ra, $ra, 0
// CHECK-NEXT:  pcalau12i $a1, %pc_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  addi.d $a1, $a1, %pc_lo12(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  lu32i.d $a1, %pc64_lo20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  lu52i.d $a1, $a1, %pc64_hi12(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  ld.d $ra, $a1, 0
// CHECK-NEXT:  jirl $ra, $ra, 0
// CHECK-NEXT:  lu12i.w $a2, %abs_hi20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  ori $a2, $a2, %abs_lo12(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  lu32i.d $a2, %abs64_lo20(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  lu52i.d $a2, $a2, %abs64_hi12(__BOLT_got_zero{{.*}})
// CHECK-NEXT:  ld.d $ra, $a2, 0
// CHECK-NEXT:  jirl $ra, $ra, 0
// CHECK-NEXT:  ret

  .text
  .globl _start
  .p2align 2
_start:
  pcalau12i $a0, %desc_pc_hi20(tdata)
  addi.d $a0, $a0, %desc_pc_lo12(tdata)
  ld.d $ra, $a0, %desc_ld(tdata)
  jirl $ra, $ra, %desc_call(tdata)
  pcalau12i $a1, %desc_pc_hi20(tdata)
  addi.d $a1, $a1, %desc_pc_lo12(tdata)
  lu32i.d $a1, %desc64_pc_lo20(tdata)
  lu52i.d $a1, $a1, %desc64_pc_hi12(tdata)
  ld.d $ra, $a1, %desc_ld(tdata)
  jirl $ra, $ra, %desc_call(tdata)
  lu12i.w $a2, %desc_hi20(tdata)
  ori $a2, $a2, %desc_lo12(tdata)
  lu32i.d $a2, %desc64_lo20(tdata)
  lu52i.d $a2, $a2, %desc64_hi12(tdata)
  ld.d $ra, $a2, %desc_ld(tdata)
  jirl $ra, $ra, %desc_call(tdata)
  ret
  .size _start, .-_start

  .section .tbss,"awT",@nobits
  .type tdata,@object
  .globl tdata
  .p2align 3
tdata:
  .quad 0
  .size tdata, .-tdata

// OBJDUMP:      <_start>:
// OBJDUMP-NEXT:     pcalau12i $a0,
// OBJDUMP-NEXT:     addi.d $a0, $a0,
// OBJDUMP-NEXT:     ld.d $ra, $a0,
// OBJDUMP-NEXT:     jirl $ra, $ra,
// OBJDUMP-NEXT:     pcalau12i $a1,
// OBJDUMP-NEXT:     addi.d $a1, $a1,
// OBJDUMP-NEXT:     lu32i.d $a1,
// OBJDUMP-NEXT:     lu52i.d $a1, $a1,
// OBJDUMP-NEXT:     ld.d $ra, $a1,
// OBJDUMP-NEXT:     jirl $ra, $ra,
// OBJDUMP-NEXT:     lu12i.w $a2,
// OBJDUMP-NEXT:     ori $a2, $a2,
// OBJDUMP-NEXT:     lu32i.d $a2,
// OBJDUMP-NEXT:     lu52i.d $a2, $a2,
// OBJDUMP-NEXT:     ld.d $ra, $a2,
// OBJDUMP-NEXT:     jirl $ra, $ra,
// OBJDUMP-NEXT:     ret
