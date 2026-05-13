// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: echo "1 _start 0 1 func_hot 0 0 1000" >> %t.fdata
// RUN: echo "1 _start 4 1 func_warm 0 0 500" >> %t.fdata
// RUN: echo "1 _start 8 1 func_cold 0 0 100" >> %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata \
// RUN:   --reorder-functions=exec-count 2>&1 | FileCheck %s
// RUN: llvm-nm -n %t.bolt | FileCheck --check-prefix=NM %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | \
// RUN:   FileCheck --check-prefix=OBJDUMP %s

// CHECK: 4 out of 4 functions in the binary (100.0%) have non-empty execution profile

// NM: func_hot
// NM-NEXT: func_warm
// NM-NEXT: func_cold
// NM-NEXT: _start

// OBJDUMP:      {{.*}} <func_hot>:
// OBJDUMP-NEXT:  ret
// OBJDUMP:      {{.*}} <func_warm>:
// OBJDUMP-NEXT:  ret
// OBJDUMP:      {{.*}} <func_cold>:
// OBJDUMP-NEXT:  ret

  .text
  .globl _start
  .p2align 2
_start:
  bl func_hot
  bl func_warm
  bl func_cold
  ret
  .size _start, .-_start

  .globl func_hot
  .p2align 2
func_hot:
  ret
  .size func_hot, .-func_hot

  .globl func_warm
  .p2align 2
func_warm:
  ret
  .size func_warm, .-func_warm

  .globl func_cold
  .p2align 2
func_cold:
  ret
  .size func_cold, .-func_cold
