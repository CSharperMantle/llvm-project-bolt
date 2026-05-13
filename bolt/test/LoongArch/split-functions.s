// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: link_fdata --no-lbr %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --split-functions 2>&1 | FileCheck %s
// RUN: llvm-nm -n %t.bolt | FileCheck --check-prefix=NM %s

// CHECK: BOLT-INFO: operating with basic samples profiling data (no brstack).

// NM: _start
// NM: _start.cold.0

  .text
  .globl _start
  .p2align 2
_start:
# FDATA: 1 _start #_start# 1000
    or $t0, $zero, $zero
    bne $t0, $zero, .Lcold
    addi.d $a0, $zero, 1
    ret
.Lcold:
    addi.d $a0, $zero, 2
    ret
    .size _start, .-_start
