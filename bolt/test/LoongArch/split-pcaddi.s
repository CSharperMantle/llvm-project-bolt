// RUN: llvm-mc --triple=loongarch64 --filetype=obj --mattr=+relax -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: link_fdata --no-lbr %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --split-functions 2>&1 | FileCheck %s
// RUN: llvm-nm -n %t.bolt | FileCheck --check-prefix=NM %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

/// Verifies PCADDI in split functions is expanded after the LoongArchRelaxationPass
/// is moved after SplitFunctions. Models gs/show_continue: a function taking its
/// own address via la.pcrel (PCADDI) from a profiled-cold block.

// CHECK: BOLT-INFO: operating with basic samples profiling data (no brstack).

// NM: f
// NM: f.cold.0

/// The cold section must have PCADDI expanded to PCALAU12I+ADDI_D.
// OBJDUMP:      <f.cold.0>:
// OBJDUMP:          pcalau12i $a0, {{[-0-9]+}}
// OBJDUMP-NEXT:     addi.d $a0, $a0, {{[-0-9]+}}
// OBJDUMP-NEXT:     st.d $a0, $sp, -8
// OBJDUMP-NEXT:     addi.d $a0, $zero, 0
// OBJDUMP-NEXT:     ret

  .text
  .globl f
  .p2align 2
f:
# FDATA: 1 f #f# 1000
  addi.d $a0, $zero, 42
  bne $a0, $zero, .Lhot_exit
  b .Lcold
.Lhot_exit:
  ret

.Lcold:
  la.pcrel $a0, f
  st.d $a0, $sp, -8
  addi.d $a0, $zero, 0
  ret
  .size f, .-f
