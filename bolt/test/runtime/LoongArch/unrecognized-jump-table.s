# REQUIRES: system-linux

# RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
# RUN: ld.lld --emit-relocs -o %t %t.o
# RUN: %t
# RUN: llvm-bolt --relocs --trap-old-code -v=1 -o %t.bolt %t 2>&1 | FileCheck %s
# RUN: llvm-readobj --symbols %t.bolt | FileCheck --check-prefix=SYMBOL %s
# RUN: %t.bolt

## The dispatch has two reaching definitions for its base register. The last
## definition in linear order is not a jump table, so BOLT cannot associate
## .LJTI with the indirect jump. Symbolization of the concrete PC-relative
## reference must independently register and rewrite .LJTI. With
## --trap-old-code, stale table entries would deterministically trap when
## false_pic is emitted at its new address.

# CHECK: BOLT-WARNING: failed to post-process indirect branches for false_pic

# SYMBOL-LABEL: Name: false_pic (
# SYMBOL:       Section: .text

  .text
  .globl _start
  .p2align 2
_start:
  ori $a0, $zero, 1
  bl false_pic
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .globl false_pic
  .p2align 2
false_pic:
  beqz   $a0, .Lfalse_base
  pcaddi $a1, %pcrel_20(.LJTI)
  b      .Ldispatch
.Lfalse_base:
  pcaddi $a1, %pcrel_20(.Lnot_table)
.Ldispatch:
  slli.d $a0, $a0, 2
  ldx.w  $a2, $a1, $a0
  add.d  $a2, $a1, $a2
  jr     $a2
.Lcase0:
  ori $a0, $zero, 1
  ret
.Lcase1:
  ori $a0, $zero, 0
  ret
  .size false_pic, .-false_pic

  .section .rodata,"a",@progbits
  .p2align 2
.LJTI:
  .reloc ., R_LARCH_32_PCREL, .Lcase0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Lcase1 + 4
  .4byte 0
.Lnot_table:
  .4byte 0
  .4byte 0
