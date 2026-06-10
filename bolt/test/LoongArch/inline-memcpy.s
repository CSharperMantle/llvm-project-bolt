// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --inline-memcpy 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT: BOLT-INFO: inlined 13 memcpy() calls

/// 1B
// OBJDUMP-LABEL: <test_1_byte>:
// OBJDUMP:       ld.bu $t0, $a1, 0
// OBJDUMP-NEXT:  st.b $t0, $a0, 0
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 2B
// OBJDUMP-LABEL: <test_2_byte>:
// OBJDUMP:       ld.hu $t0, $a1, 0
// OBJDUMP-NEXT:  st.h $t0, $a0, 0
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 4B
// OBJDUMP-LABEL: <test_4_byte>:
// OBJDUMP:       ld.wu $t0, $a1, 0
// OBJDUMP-NEXT:  st.w $t0, $a0, 0
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 8B
// OBJDUMP-LABEL: <test_8_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 9B
// OBJDUMP-LABEL: <test_9_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.bu $t1, $a1, 8
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.b $t1, $a0, 8
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 15B
// OBJDUMP-LABEL: <test_15_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.wu $t1, $a1, 8
// OBJDUMP-NEXT:  ld.hu $t2, $a1, 12
// OBJDUMP-NEXT:  ld.bu $t3, $a1, 14
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.w $t1, $a0, 8
// OBJDUMP-NEXT:  st.h $t2, $a0, 12
// OBJDUMP-NEXT:  st.b $t3, $a0, 14
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 16B
// OBJDUMP-LABEL: <test_16_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.d $t1, $a1, 8
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.d $t1, $a0, 8
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 24B
// OBJDUMP-LABEL: <test_24_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.d $t1, $a1, 8
// OBJDUMP-NEXT:  ld.d $t2, $a1, 16
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.d $t1, $a0, 8
// OBJDUMP-NEXT:  st.d $t2, $a0, 16
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 30B
// OBJDUMP-LABEL: <test_30_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.d $t1, $a1, 8
// OBJDUMP-NEXT:  ld.d $t2, $a1, 16
// OBJDUMP-NEXT:  ld.wu $t3, $a1, 24
// OBJDUMP-NEXT:  ld.hu $t4, $a1, 28
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.d $t1, $a0, 8
// OBJDUMP-NEXT:  st.d $t2, $a0, 16
// OBJDUMP-NEXT:  st.w $t3, $a0, 24
// OBJDUMP-NEXT:  st.h $t4, $a0, 28
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 32B
// OBJDUMP-LABEL: <test_32_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.d $t1, $a1, 8
// OBJDUMP-NEXT:  ld.d $t2, $a1, 16
// OBJDUMP-NEXT:  ld.d $t3, $a1, 24
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.d $t1, $a0, 8
// OBJDUMP-NEXT:  st.d $t2, $a0, 16
// OBJDUMP-NEXT:  st.d $t3, $a0, 24
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 40B
// OBJDUMP-LABEL: <test_40_byte>:
// OBJDUMP:       ld.d $t0, $a1, 0
// OBJDUMP-NEXT:  ld.d $t1, $a1, 8
// OBJDUMP-NEXT:  ld.d $t2, $a1, 16
// OBJDUMP-NEXT:  ld.d $t3, $a1, 24
// OBJDUMP-NEXT:  ld.d $t4, $a1, 32
// OBJDUMP-NEXT:  st.d $t0, $a0, 0
// OBJDUMP-NEXT:  st.d $t1, $a0, 8
// OBJDUMP-NEXT:  st.d $t2, $a0, 16
// OBJDUMP-NEXT:  st.d $t3, $a0, 24
// OBJDUMP-NEXT:  st.d $t4, $a0, 32
// OBJDUMP-NOT:   bl{{.*}}memcpy

/// 128-byte: should NOT be inlined
// OBJDUMP-LABEL: <test_128_byte>:
// OBJDUMP:       bl{{.*}}memcpy

/// Size in register: should NOT be inlined
// OBJDUMP-LABEL: <test_register_size>:
// OBJDUMP:       bl{{.*}}memcpy

	.text
	.globl _start
	.type _start,@function
_start:
	bl test_0_byte
	bl test_1_byte
	bl test_2_byte
	bl test_4_byte
	bl test_8_byte
	bl test_9_byte
	bl test_15_byte
	bl test_16_byte
	bl test_24_byte
	bl test_30_byte
	bl test_32_byte
	bl test_40_byte
	bl test_64_byte
	bl test_128_byte
	bl test_register_size
	li.d $a7, 93
	syscall 0
	.size _start, .-_start

	.globl test_0_byte
	.type test_0_byte,@function
test_0_byte:
	addi.d $sp, $sp, -32
	st.d $ra, $sp, 24
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 8
	addi.d $a2, $zero, 0
	bl memcpy
	ld.d $ra, $sp, 24
	addi.d $sp, $sp, 32
	jr $ra
	.size test_0_byte, .-test_0_byte

	.globl test_1_byte
	.type test_1_byte,@function
test_1_byte:
	addi.d $sp, $sp, -32
	st.d $ra, $sp, 24
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 8
	addi.d $a2, $zero, 1
	bl memcpy
	ld.d $ra, $sp, 24
	addi.d $sp, $sp, 32
	jr $ra
	.size test_1_byte, .-test_1_byte

	.globl test_2_byte
	.type test_2_byte,@function
test_2_byte:
	addi.d $sp, $sp, -32
	st.d $ra, $sp, 24
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 8
	addi.d $a2, $zero, 2
	bl memcpy
	ld.d $ra, $sp, 24
	addi.d $sp, $sp, 32
	jr $ra
	.size test_2_byte, .-test_2_byte

	.globl test_4_byte
	.type test_4_byte,@function
test_4_byte:
	addi.d $sp, $sp, -32
	st.d $ra, $sp, 24
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 8
	addi.d $a2, $zero, 4
	bl memcpy
	ld.d $ra, $sp, 24
	addi.d $sp, $sp, 32
	jr $ra
	.size test_4_byte, .-test_4_byte

	.globl test_8_byte
	.type test_8_byte,@function
test_8_byte:
	addi.d $sp, $sp, -32
	st.d $ra, $sp, 24
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 8
	addi.d $a2, $zero, 8
	bl memcpy
	ld.d $ra, $sp, 24
	addi.d $sp, $sp, 32
	jr $ra
	.size test_8_byte, .-test_8_byte

	.globl test_9_byte
	.type test_9_byte,@function
test_9_byte:
	addi.d $sp, $sp, -48
	st.d $ra, $sp, 40
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 24
	addi.d $a2, $zero, 9
	bl memcpy
	ld.d $ra, $sp, 40
	addi.d $sp, $sp, 48
	jr $ra
	.size test_9_byte, .-test_9_byte

	.globl test_15_byte
	.type test_15_byte,@function
test_15_byte:
	addi.d $sp, $sp, -48
	st.d $ra, $sp, 40
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 24
	addi.d $a2, $zero, 15
	bl memcpy
	ld.d $ra, $sp, 40
	addi.d $sp, $sp, 48
	jr $ra
	.size test_15_byte, .-test_15_byte

	.globl test_16_byte
	.type test_16_byte,@function
test_16_byte:
	addi.d $sp, $sp, -48
	st.d $ra, $sp, 40
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 24
	addi.d $a2, $zero, 16
	bl memcpy
	ld.d $ra, $sp, 40
	addi.d $sp, $sp, 48
	jr $ra
	.size test_16_byte, .-test_16_byte

	.globl test_24_byte
	.type test_24_byte,@function
test_24_byte:
	addi.d $sp, $sp, -64
	st.d $ra, $sp, 56
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 32
	addi.d $a2, $zero, 24
	bl memcpy
	ld.d $ra, $sp, 56
	addi.d $sp, $sp, 64
	jr $ra
	.size test_24_byte, .-test_24_byte

	.globl test_30_byte
	.type test_30_byte,@function
test_30_byte:
	addi.d $sp, $sp, -96
	st.d $ra, $sp, 88
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 48
	addi.d $a2, $zero, 30
	bl memcpy
	ld.d $ra, $sp, 88
	addi.d $sp, $sp, 96
	jr $ra
	.size test_30_byte, .-test_30_byte

	.globl test_32_byte
	.type test_32_byte,@function
test_32_byte:
	addi.d $sp, $sp, -80
	st.d $ra, $sp, 72
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 40
	addi.d $a2, $zero, 32
	bl memcpy
	ld.d $ra, $sp, 72
	addi.d $sp, $sp, 80
	jr $ra
	.size test_32_byte, .-test_32_byte

	.globl test_40_byte
	.type test_40_byte,@function
test_40_byte:
	addi.d $sp, $sp, -96
	st.d $ra, $sp, 88
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 40
	addi.d $a2, $zero, 40
	bl memcpy
	ld.d $ra, $sp, 88
	addi.d $sp, $sp, 96
	jr $ra
	.size test_40_byte, .-test_40_byte

	.globl test_64_byte
	.type test_64_byte,@function
test_64_byte:
	addi.d $sp, $sp, -128
	st.d $ra, $sp, 120
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 48
	addi.d $a2, $zero, 64
	bl memcpy
	ld.d $ra, $sp, 120
	addi.d $sp, $sp, 128
	jr $ra
	.size test_64_byte, .-test_64_byte

	.globl test_128_byte
	.type test_128_byte,@function
test_128_byte:
	addi.d $sp, $sp, -256
	st.d $ra, $sp, 248
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 128
	addi.d $a2, $zero, 128
	bl memcpy
	ld.d $ra, $sp, 248
	addi.d $sp, $sp, 256
	jr $ra
	.size test_128_byte, .-test_128_byte

	.globl test_register_size
	.type test_register_size,@function
test_register_size:
	addi.d $sp, $sp, -32
	st.d $ra, $sp, 24
	addi.d $a1, $sp, 16
	addi.d $a0, $sp, 8
	addi.d $a2, $zero, 8
	ori $a2, $t0, 0
	bl memcpy
	ld.d $ra, $sp, 24
	addi.d $sp, $sp, 32
	jr $ra
	.size test_register_size, .-test_register_size

	.globl memcpy
	.type memcpy,@function
memcpy:
	jr $ra
	.size memcpy, .-memcpy
