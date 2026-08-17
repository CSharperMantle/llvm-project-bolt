/// Verify that double-jump elimination does not treat an indirect jump with a
/// single unique CFG successor as a direct branch.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --peepholes=all --print-peepholes \
// RUN:   --print-only=jump_table_tail 2>&1 | FileCheck %s

// CHECK-LABEL: Binary Function "jump_table_tail" after peepholes
// CHECK:         jr $a2 # JUMPTABLE @
// CHECK:         b callee # TAILCALL
// CHECK:       End of Function "jump_table_tail"

.text
.globl _start
.p2align 2
_start:
  ret
.size _start, .-_start

.globl jump_table_tail
.p2align 2
jump_table_tail:
  pcaddi $a1, %pcrel_20(.LJTI)
  ori $a0, $zero, 0
  slli.d $a0, $a0, 3
  ldx.d $a2, $a1, $a0
  jr $a2
.Lcase:
  b callee
.size jump_table_tail, .-jump_table_tail

.globl callee
.p2align 2
callee:
  ret
.size callee, .-callee

.section .rodata,"a",@progbits
.p2align 3
.LJTI:
  .dword .Lcase
  .dword .Lcase
  .dword .Lcase
