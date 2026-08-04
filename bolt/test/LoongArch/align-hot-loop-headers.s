# RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
# RUN: link_fdata %s %t.o %t.fdata --nmtool llvm-nm
# RUN: ld.lld --emit-relocs -e _start -o %t %t.o
# RUN: llvm-strip --strip-unneeded %t
#
# RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata --relocs \
# RUN:   --reorder-blocks=none --align-hot-loop-headers --hot-loop-alignment=16 \
# RUN:   --print-finalized 2>&1 | FileCheck %s --check-prefix=CHECK
# RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck %s --check-prefix=OBJDUMP

# CHECK-LABEL: Binary Function "_start
# CHECK: (4 instructions, align : 16)
# CHECK-LABEL: Binary Function "hot_fallthrough_loop"
# CHECK-NOT: align : 16
# CHECK-LABEL: Binary Function "cold_loop"
# CHECK-NOT: align : 16

# OBJDUMP-LABEL:  <_start>:
# OBJDUMP-NEXT:     addi.d $a0, $a0, 1
# OBJDUMP-NEXT:     nop
# OBJDUMP-NEXT:     nop
# OBJDUMP-NEXT:     nop
# OBJDUMP-NEXT:     addi.d $t0, $t0, -1

# OBJDUMP-LABEL:  <hot_fallthrough_loop>:
# OBJDUMP-NEXT:     beqz
# OBJDUMP-NEXT:     b
# OBJDUMP-NEXT:     addi.d
# OBJDUMP-NEXT:     beqz
# OBJDUMP-NEXT:     addi.d
# OBJDUMP-NEXT:     b
# OBJDUMP-NEXT:     ret

# OBJDUMP-LABEL:  <cold_loop>:
# OBJDUMP-NEXT:     addi.d $a0, $a0, 1
# OBJDUMP-NEXT:     addi.d $t0, $t0, -1
# OBJDUMP-NEXT:     addi.d $t2, $t2, 1
# OBJDUMP-NEXT:     addi.d $t3, $t3, 1
# OBJDUMP-NEXT:     bnez
# OBJDUMP-NEXT:     ret

  .text
  .globl _start
  .type _start, @function
  .p2align 2
_start:
  addi.d $a0, $a0, 1
Lentry_to_hot:
  b Lhot_header
Lhot_header:
  addi.d $t0, $t0, -1
  addi.d $t2, $t2, 1
  addi.d $t3, $t3, 1
Lhot_backedge:
  bnez $t0, Lhot_header
Lhot_exit:
  ret
  .size _start, .-_start

# FDATA: 0 [unknown] 0 1 _start 0 0 100
# FDATA: 1 _start #Lentry_to_hot# 1 _start #Lhot_header# 0 100
# FDATA: 1 _start #Lhot_backedge# 1 _start #Lhot_header# 0 100000
# FDATA: 1 _start #Lhot_backedge# 1 _start #Lhot_exit# 0 100

  .globl hot_fallthrough_loop
  .type hot_fallthrough_loop, @function
  .p2align 2
hot_fallthrough_loop:
Lfall_entry_branch:
  beqz $a0, Lfall_exit
Lfall_to_header:
  b Lfall_header
Lfall_latch:
  addi.d $t1, $t1, -1
Lfall_branch:
  beqz $t1, Lfall_exit
Lfall_header:
  addi.d $t0, $t0, 1
Lfall_to_latch:
  b Lfall_latch
Lfall_exit:
  ret
  .size hot_fallthrough_loop, .-hot_fallthrough_loop

# FDATA: 0 [unknown] 0 1 hot_fallthrough_loop 0 0 100
# FDATA: 1 hot_fallthrough_loop #Lfall_entry_branch# 1 hot_fallthrough_loop #Lfall_exit# 0 1
# FDATA: 1 hot_fallthrough_loop #Lfall_entry_branch# 1 hot_fallthrough_loop #Lfall_to_header# 0 100
# FDATA: 1 hot_fallthrough_loop #Lfall_to_header# 1 hot_fallthrough_loop #Lfall_header# 0 100
# FDATA: 1 hot_fallthrough_loop #Lfall_branch# 1 hot_fallthrough_loop #Lfall_exit# 0 100
# FDATA: 1 hot_fallthrough_loop #Lfall_branch# 1 hot_fallthrough_loop #Lfall_header# 0 100000
# FDATA: 1 hot_fallthrough_loop #Lfall_to_latch# 1 hot_fallthrough_loop #Lfall_latch# 0 100100

  .globl cold_loop
  .type cold_loop, @function
  .p2align 2
cold_loop:
  addi.d $a0, $a0, 1
Lcold_entry:
  b Lcold_header
Lcold_header:
  addi.d $t0, $t0, -1
  addi.d $t2, $t2, 1
  addi.d $t3, $t3, 1
Lcold_backedge:
  bnez $t0, Lcold_header
Lcold_exit:
  ret
  .size cold_loop, .-cold_loop

# FDATA: 0 [unknown] 0 1 cold_loop 0 0 100
# FDATA: 1 cold_loop #Lcold_entry# 1 cold_loop #Lcold_header# 0 100
# FDATA: 1 cold_loop #Lcold_backedge# 1 cold_loop #Lcold_header# 0 10
# FDATA: 1 cold_loop #Lcold_backedge# 1 cold_loop #Lcold_exit# 0 100

## Keep one relocation against code so BOLT can exercise relocation-mode
## alignment in this reduced executable.
  .globl reloc_anchor
  .type reloc_anchor, @function
reloc_anchor:
  bl hot_fallthrough_loop
  ret
  .size reloc_anchor, .-reloc_anchor
