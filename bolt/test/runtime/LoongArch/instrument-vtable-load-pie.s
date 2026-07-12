// REQUIRES: system-linux,bolt-runtime

// RUN: %clang %cflags -Wl,-q -o %t.exe %s

// RUN: llvm-bolt %t.exe --instrument --instrument-load-profiles \
// RUN:   --instrumentation-file=%t.fdata -o %t.instr
// RUN: %t.instr
// RUN: cat %t.fdata | FileCheck %s --check-prefix=CHECK-MEMINFO

// RUN: llvm-bolt %t.exe --data %t.fdata --icp=calls \
// RUN:   --icp-calls-topn=1 --icp-eliminate-loads --print-icp -v=1 \
// RUN:   -o %t.bolt 2>&1 | FileCheck %s --check-prefix=CHECK-ICP
// RUN: %t.bolt | FileCheck %s --check-prefix=CHECK-OUTPUT

/// The records below correspond to the three method loads. The effective
/// addresses are vtable slot addresses.
// CHECK-MEMINFO: 4 caller 10 3 [unknown] {{[0-9a-f]+}} 1
// CHECK-MEMINFO: 4 caller_ldptr 10 3 [unknown] {{[0-9a-f]+}} 1
// CHECK-MEMINFO: 4 caller_same_reg 10 3 [unknown] {{[0-9a-f]+}} 1
// CHECK-MEMINFO-NOT: 4 caller_nonvirtual

// CHECK-ICP-DAG: ICP found method = {{[0-9a-f]+}}/hot
// CHECK-ICP-DAG: ICP found virtual method call in caller at
// CHECK-ICP-DAG: ICP found virtual method call in caller_ldptr at
// CHECK-ICP-DAG: ICP found virtual method call in caller_same_reg at
// CHECK-ICP-DAG: ICP succeeded in caller @
// CHECK-ICP-DAG: ICP succeeded in caller_ldptr @
// CHECK-ICP-DAG: ICP succeeded in caller_same_reg @
// CHECK-ICP: ICP number of method load elimination candidates = 3
// CHECK-ICP: ICP percentage of method calls candidates that have loads eliminated = 100.0%
// CHECK-OUTPUT: 42

    .text
    .globl main
    .type main, @function
    .p2align 2
main:
    addi.d $sp, $sp, -32
    st.d $ra, $sp, 24

    bl caller_nonvirtual
    bl caller
    st.d $a0, $sp, 0
    bl caller_ldptr
    st.d $a0, $sp, 8
    bl caller_same_reg
    ld.d $a1, $sp, 0
    add.d $a0, $a0, $a1
    ld.d $a1, $sp, 8
    add.d $a1, $a0, $a1
    la.local $a0, .Lformat
    bl printf
    li.d $a0, 0

    ld.d $ra, $sp, 24
    addi.d $sp, $sp, 32
    ret
    .size main, .-main

    .globl caller
    .type caller, @function
    .p2align 2
caller:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 8

    la.local $t0, obj
    ld.d $t0, $t0, 0
    ld.d $t1, $t0, 16
    jirl $ra, $t1, 0

    ld.d $ra, $sp, 8
    addi.d $sp, $sp, 16
    ret
    .size caller, .-caller

    .globl caller_ldptr
    .type caller_ldptr, @function
    .p2align 2
caller_ldptr:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 8

    la.local $t0, obj
    ld.d $t0, $t0, 0
    ldptr.d $t1, $t0, 16
    jirl $ra, $t1, 0

    ld.d $ra, $sp, 8
    addi.d $sp, $sp, 16
    ret
    .size caller_ldptr, .-caller_ldptr

    .globl caller_same_reg
    .type caller_same_reg, @function
    .p2align 2
caller_same_reg:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 8

    la.local $t0, obj
    ld.d $t0, $t0, 0
    ld.d $t0, $t0, (8*(3-1))
    jirl $ra, $t0, 0

    ld.d $ra, $sp, 8
    addi.d $sp, $sp, 16
    ret
    .size caller_same_reg, .-caller_same_reg

    .globl caller_nonvirtual
    .type caller_nonvirtual, @function
    .p2align 2
caller_nonvirtual:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 8

    la.local $t0, hot
    jirl $ra, $t0, 0

    ld.d $ra, $sp, 8
    addi.d $sp, $sp, 16
    ret
    .size caller_nonvirtual, .-caller_nonvirtual

    .local hot
    .type hot, @function
    .p2align 2
hot:
    li.w $a0, 14
    ret
    .size hot, .-hot

    .data
    .p2align 3
obj:
    .dword vtable

    .section .rodata,"a",@progbits
.Lformat:
    .asciz "%d\n"

    .section .data.rel.ro,"aw",@progbits
    .p2align 3
vtable:
    .dword 0
    .dword 0
    .dword hot
