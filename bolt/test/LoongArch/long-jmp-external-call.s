/// Check that LongJmp relaxes a stub targeting an external LoongArch B26 call
/// without clobbering the original call return address.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs --unresolved-symbols=ignore-all -pie -o %t %t.o
// RUN: llvm-bolt %t -o %t.bolt --align-text=0x10000000 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// BOLT-NOT: PLEASE submit a bug report
// BOLT-NOT: BOLT-ERROR
// BOLT: BOLT-INFO: Inserted 1 stubs in the hot area and 0 stubs in the cold area.

// OBJDUMP:      <main>:
// OBJDUMP:      bl 8 <main+0x10>
// OBJDUMP-NEXT: ret
// OBJDUMP-NEXT: pcaddu18i $r21,
// OBJDUMP-NEXT: jirl $zero, $r21,
// OBJDUMP-NOT:  jirl $ra, $r21, 0

  .text

  .globl main
  .type main, @function
main:
  pcalau12i $a0, %pc_hi20(msg)
  addi.d $a0, $a0, %pc_lo12(msg)
  bl printf
  ret
  .size main, .-main

  .section .rodata
msg:
  .asciz "x\n"
