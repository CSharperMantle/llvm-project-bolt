// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs --image-base=0x120000000 -o %t.exe %t.o
// RUN: not %t.exe
// RUN: llvm-bolt %t.exe --hot-text --huge-page-size=33554432 -o %t.bolt
// RUN: llvm-readelf --symbols %t.bolt | FileCheck %s --check-prefix=CHECK-SYM
// RUN: llvm-readelf --hex-dump=.bolt.hugify.data %t.bolt | FileCheck %s --check-prefix=CHECK-DATA
// RUN: %t.bolt

// CHECK-SYM: 8 OBJECT GLOBAL DEFAULT {{[0-9]+}} __bolt_hugify_page_size

// CHECK-DATA:      Hex dump of section '.bolt.hugify.data':
// CHECK-DATA-NEXT: 0x{{[0-9a-f]+}} 00000002 00000000

  .text
  .globl _start
  .type _start, @function
  .p2align 2
_start:
  bl check_page_size
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .globl check_page_size
  .type check_page_size, @function
  .p2align 2
check_page_size:
  pcalau12i $a0, %pc_hi20(__bolt_hugify_page_size)
  ld.d $a0, $a0, %pc_lo12(__bolt_hugify_page_size)
  li.d $t0, 33554432
  sub.d $a0, $a0, $t0
  sltu $a0, $zero, $a0
  ret
  .size check_page_size, .-check_page_size

/// References to this symbol will be redirected by BOLT to the
/// corresponding symbol in .bolt.hugify.data section.
  .section .rodata,"a",@progbits
  .p2align 3
  .globl __bolt_hugify_page_size
  .type __bolt_hugify_page_size, @object
__bolt_hugify_page_size:
  .quad 2097152
  .size __bolt_hugify_page_size, 8
