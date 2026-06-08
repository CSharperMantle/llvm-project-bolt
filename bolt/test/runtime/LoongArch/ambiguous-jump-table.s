# REQUIRES: system-linux,bolt-runtime

# RUN: %clang %cflags -fno-pie -Wl,-no-pie -Wl,-q -o %t.exe %s
# RUN: llvm-bolt --instrument --instrumentation-file=%t.fdata \
# RUN:   --print-only=main --print-finalized -o %t.instr %t.exe 2>&1 \
# RUN:   | FileCheck %s
# RUN: %t.instr
# RUN: llvm-bolt %t.exe --data %t.fdata --reorder-blocks=ext-tsp -o %t.bolt
# RUN: %t.bolt

# CHECK: Jump table .LduplicatedJT

    .text
    .globl main
    .type main, @function
main:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 8

    ## First dispatch: index 0 -> .Lret0
    pcaddi $t1, %pcrel_20(.LJT0)
    slli.d $t0, $zero, 3
    ldx.d $t2, $t1, $t0
    jr $t2
.Lret0:
    ## Second dispatch: index 1 -> .Lret1
    pcaddi $t1, %pcrel_20(.LJT0)
    li.d $t0, 1
    slli.d $t0, $t0, 3
    ldx.d $t2, $t1, $t0
    jr $t2
.Lret1:
    ld.d $ra, $sp, 8
    addi.d $sp, $sp, 16
    ori $a0, $zero, 0
    ret
    .size main, .-main

    .section .rodata,"a",@progbits
    .p2align 3
.LJT0:
    .dword .Lret0
    .dword .Lret1
