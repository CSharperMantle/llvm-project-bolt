// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs --image-base=0x120000000 -o %t.exe %t.o
// RUN: llvm-bolt %t.exe --hugify --huge-page-size=33554432 -o %t.bolt
// RUN: llvm-readelf --symbols %t.bolt | FileCheck %s --check-prefix=CHECK-SYM
// RUN: llvm-readelf --hex-dump=.bolt.hugify.data %t.bolt | FileCheck %s --check-prefix=CHECK-DATA
// RUN: %t.bolt
// RUN: llvm-bolt %t.exe --hot-text --huge-page-size=33554432 -o %t.hot-text.bolt
// RUN: llvm-readelf --symbols %t.hot-text.bolt | FileCheck %s --check-prefix=CHECK-SYM
// RUN: llvm-readelf --hex-dump=.bolt.hugify.data %t.hot-text.bolt | FileCheck %s --check-prefix=CHECK-DATA
// RUN: %t.hot-text.bolt

// CHECK-SYM: 8 OBJECT GLOBAL DEFAULT {{[0-9]+}} __bolt_hugify_page_size

// CHECK-DATA:      Hex dump of section '.bolt.hugify.data':
// CHECK-DATA-NEXT: 0x{{[0-9a-f]+}} 00000002 00000000

  .text
  .globl _start
  .p2align 2
_start:
  pcalau12i $a0, %pc_hi20(.Lvalue)
  ld.d $a0, $a0, %pc_lo12(.Lvalue)
  li.d $a7, 93
  syscall 0
  .size _start, .-_start

  .data
  .p2align 3
.Lvalue:
  .quad 0
