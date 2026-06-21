# RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
# RUN: ld.lld --emit-relocs -o %t %t.o
# RUN: llvm-bolt --frame-opt=all -o %t.bolt %t | FileCheck %s
# RUN: llvm-objdump -d %t.bolt | FileCheck %s --check-prefix=OBJDUMP

# CHECK: BOLT-INFO: FOP optimized 2 redundant load(s)

# OBJDUMP-LABEL: <_start>:
# OBJDUMP:         addi.d $sp, $sp, -16
# OBJDUMP-NEXT:    st.d   $s0, $sp, 0
# OBJDUMP-NEXT:    st.d   $s1, $sp, 8
# OBJDUMP-NEXT:    addi.d $sp, $sp, 16

  .text
  .globl _start
  .type _start, %function
_start:
  .cfi_startproc
  addi.d  $sp, $sp, -16
  st.d    $s0, $sp, 0
  st.d    $s1, $sp, 8
  ld.d    $s0, $sp, 0   # to be eliminated
  ld.d    $s1, $sp, 8   # to be eliminated
  addi.d  $sp, $sp, 16
  li.d    $a0, 0
  li.d    $a7, 93
  syscall 0
  ret
  .cfi_endproc
  .size _start, .-_start
