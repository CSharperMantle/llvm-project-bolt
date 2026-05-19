/// Check LoongArch jump-table indirect-call promotion.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t.pre %t.o
// RUN: link_fdata %s %t.pre %t.fdata
// RUN: llvm-strip --strip-unneeded %t.o
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt %t -o %t.default.bolt --relocs --data=%t.fdata \
// RUN:   --icp=jump-tables --icp-top-callsites=0 --icp-jt-topn=1 \
// RUN:   --print-icp -v=1 --print-only=jt_default 2>&1 \
// RUN:   | FileCheck --check-prefix=DEFAULT %s
// RUN: llvm-bolt %t -o %t.target.bolt --relocs --data=%t.fdata \
// RUN:   --icp=jump-tables --icp-top-callsites=0 --icp-jt-topn=1 \
// RUN:   --icp-jump-tables-targets --print-icp -v=1 \
// RUN:   --print-only=jt_target 2>&1 \
// RUN:   | FileCheck --check-prefix=TARGET %s

// DEFAULT: BOLT-INFO: ICP succeeded in jt_default
// DEFAULT-LABEL: Binary Function "jt_default" after indirect-call-promotion
// DEFAULT:      pcalau12i $r21, %pc_hi20(.L{{.*}})
// DEFAULT-NEXT: addi.d $r21, $r21, %pc_lo12(.L{{.*}})
// DEFAULT-NEXT: beq $a2, $r21, .L{{.*}}
// DEFAULT:      jr $a2
// DEFAULT:      End of Function "jt_default"

// TARGET: BOLT-INFO: ICP succeeded in jt_target
// TARGET-LABEL: Binary Function "jt_target" after indirect-call-promotion
// TARGET:      pcalau12i $r21, %pc_hi20(.L{{.*}})
// TARGET-NEXT: addi.d $r21, $r21, %pc_lo12(.L{{.*}})
// TARGET-NEXT: beq $a2, $r21, .L{{.*}}
// TARGET:      jr $a2
// TARGET:      End of Function "jt_target"

  .text
  .globl _start
  .p2align 2
_start:
  ori $a0, $zero, 1
  bl jt_default
  ori $a0, $zero, 1
  bl jt_target
  ret
  .size _start, .-_start

  .globl jt_default
  .p2align 2
jt_default:
  pcaddi $a1, %pcrel_20(.LJTI_default)
  slli.d $a0, $a0, 3
  ldx.d $a2, $a1, $a0
.Ljt_default_branch:
  jr $a2
// FDATA: 1 jt_default c 1 jt_default 18 0 100

.Ldefault_case0:
  ori $a0, $zero, 0
  ret
.Ldefault_case1:
  ori $a0, $zero, 1
  ret
.Ldefault_case2:
  ori $a0, $zero, 2
  ret
  .size jt_default, .-jt_default

  .globl jt_target
  .p2align 2
jt_target:
  pcaddi $a1, %pcrel_20(.LJTI_target)
  slli.d $a0, $a0, 2
  ldx.w $a2, $a1, $a0
  add.d $a2, $a1, $a2
.Ljt_target_branch:
  jr $a2
// FDATA: 1 jt_target 10 1 jt_target 1c 0 100

.Ltarget_case0:
  ori $a0, $zero, 0
  ret
.Ltarget_case1:
  ori $a0, $zero, 1
  ret
.Ltarget_case2:
  ori $a0, $zero, 2
  ret
  .size jt_target, .-jt_target

  .section .rodata,"a",@progbits
  .p2align 3
.LJTI_default:
  .dword .Ldefault_case0
  .dword .Ldefault_case1
  .dword .Ldefault_case2

  .p2align 2
.LJTI_target:
  .reloc ., R_LARCH_32_PCREL, .Ltarget_case0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Ltarget_case1 + 4
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Ltarget_case2 + 8
  .4byte 0
