# REQUIRES: system-linux

# RUN: llvm-mc --triple=loongarch64 --filetype=obj --mattr=+relax -o %t.o %s
# RUN: ld.lld --emit-relocs -o %t %t.o
# RUN: llvm-objdump -dr --no-show-raw-insn %t | FileCheck --check-prefix=INPUT %s
# RUN: %t
# RUN: llvm-bolt --relocs --trap-old-code --skip-funcs=_start -o %t.bolt %t
# RUN: llvm-readobj --symbols %t.bolt | FileCheck --check-prefix=READOBJ %s
# RUN: %t.bolt

## _start is not emitted, so BOLT cannot expand its linker-relaxed PCADDI back
## into PCALAU12I+ADDI_D without changing the function's layout. Moving target
## would give target two different addresses: the old one materialized by the
## PCADDI and the new one written to target_ptr. Keep target at its original
## address so all address references remain equal.

# INPUT:  <_start>:
# INPUT:    pcaddi $a0,
# INPUT:      R_LARCH_PCREL20_S2 target

# READOBJ:       Name: target (
# READOBJ-NEXT:  Value:
# READOBJ-NEXT:  Size:
# READOBJ-NEXT:  Binding:
# READOBJ-NEXT:  Type:
# READOBJ-NEXT:  Other:
# READOBJ-NEXT:  Section: .bolt.org.text

  .text
  .globl _start
  .p2align 2
_start:
  la.pcrel $a0, target
  pcalau12i $a1, %pc_hi20(target_ptr)
  ld.d $a1, $a1, %pc_lo12(target_ptr)
  xor $a0, $a0, $a1
  sltu $a0, $zero, $a0
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .globl target
  .p2align 2
target:
  ret
  .size target, .-target

  .data
  .p2align 3
target_ptr:
  .quad target
