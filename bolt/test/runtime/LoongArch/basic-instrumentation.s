# REQUIRES: system-linux,bolt-runtime

# RUN: %clang %cflags -Wl,-q -o %t.exe %s
# RUN: llvm-bolt --instrument --instrumentation-file=%t.fdata -o %t.instr %t.exe

## Run the profiled binary and check that the profile reports at least that `f`
## has been called.
# RUN: rm -f %t.fdata
# RUN: %t.instr
# RUN: cat %t.fdata | FileCheck %s
# CHECK: f 0 0 1{{$}}

## Check BOLT works with this profile
# RUN: llvm-bolt --data %t.fdata --reorder-blocks=cache -o %t.bolt %t.exe

    .text
    .globl main
    .type main, @function
main:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 8
    bl f
    ld.d $ra, $sp, 8
    addi.d $sp, $sp, 16
    li.d $t0, 0xdeadbeef
    sub.d $a0, $a0, $t0
    sltu $a0, $zero, $a0
    ret
    .size main, .-main

    .globl f
    .type f, @function
f:
    li.d $a0, 0xdeadbeef
    ret
    .size f, .-f
