# RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
# RUN: ld.lld --emit-relocs -o %t %t.o
# RUN: llvm-bolt --frame-opt=all --frame-opt-rm-stores -o %t.bolt %t 2>&1 | FileCheck %s --check-prefix=BOLT
# RUN: llvm-objdump -d %t.bolt | FileCheck %s --check-prefix=OBJDUMP

## CFA-based recovery must require all incoming paths to have the same CFA
## origin. The epilogue in cfa_fallback is both a secondary entry with the
## initial CFA and an internal target reached with an allocated frame.
##
## The check must not affect an earlier SP-based recovery. The merged entry in
## sp_recovery has a concrete SP offset, so its redundant load and then-unused
## store are still removed.

# BOLT: BOLT-INFO: FOP optimized 1 redundant load(s) and 1 unused store(s)

# OBJDUMP-LABEL: <cfa_fallback>:
# OBJDUMP:         addi.d $sp, $sp, -16
# OBJDUMP-NEXT:    st.d   $s0, $sp, 0
# OBJDUMP:         ld.d   $s0, $sp, 0
# OBJDUMP-NEXT:    addi.d $sp, $sp, 16
# OBJDUMP-NEXT:    ret

# OBJDUMP-LABEL: <sp_recovery>:
# OBJDUMP-NOT:     st.d   $s0, $sp, -8
# OBJDUMP-NOT:     ld.d   $s0, $sp, -8
# OBJDUMP:         ret

  .text
  .globl _start
  .type _start, %function
_start:
  bl cfa_fallback
  bl sp_recovery
  li.d $a0, 0
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .globl cfa_fallback
  .type cfa_fallback, %function
cfa_fallback:
  addi.d $sp, $sp, -16
  st.d $s0, $sp, 0
  addi.d $s0, $a0, 1
  beqz $a0, .cfa_epilogue
  addi.d $s0, $s0, 1
  .local .cfa_epilogue
.cfa_epilogue:
  ld.d $s0, $sp, 0
  addi.d $sp, $sp, 16
  ret
  .size cfa_fallback, .-cfa_fallback

  .globl sp_recovery
  .type sp_recovery, %function
sp_recovery:
  beqz $a0, .sp_entry
  addi.d $a0, $a0, 1
  .local .sp_entry
.sp_entry:
  st.d $s0, $sp, -8
  ld.d $s0, $sp, -8
  ret
  .size sp_recovery, .-sp_recovery
