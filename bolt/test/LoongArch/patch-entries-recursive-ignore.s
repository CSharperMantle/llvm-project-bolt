/// Check that entry redirections are emitted only after all functions have
/// been checked for patchability. An entry patch emitted for target before
/// the patchability scan would redirect target to itself.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj --mattr=+relax -o %t.o %s
// RUN: ld.lld --emit-relocs -e _start -o %t %t.o
// RUN: llvm-objdump -dr --no-show-raw-insn %t | FileCheck --check-prefix=INPUT %s
// RUN: llvm-bolt %t -o %t.bolt --force-patch --skip-funcs=_start 2>&1 | FileCheck --check-prefix=BOLT %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OUTPUT %s

// INPUT-LABEL: <target>:
// INPUT-NEXT:    ret
// INPUT-NEXT:    nop
// INPUT-LABEL: <late_ignored>:
// INPUT-NEXT:    pcaddi $a0,
// INPUT:           R_LARCH_PCREL20_S2 target

// BOLT: BOLT-WARNING: failed to patch entries in late_ignored
// BOLT: BOLT-WARNING: unable to expand PCADDI in function late_ignored that references target. Will not optimize the target

// OUTPUT-LABEL: <target>:
// OUTPUT-NEXT:    ret
// OUTPUT-NEXT:    nop
// OUTPUT-NOT:   <target.org.0>:

  .text

  .globl target
  .type target, @function
  .p2align 2
target:
  ret
  nop
  .size target, .-target

/// Linker relaxation makes this function 4B (1 insn) long, which is too small
/// for the 2-insn entry redirection under --force-patch.
  .globl late_ignored
  .type late_ignored, @function
  .p2align 2
late_ignored:
  la.pcrel $a0, target
  .size late_ignored, .-late_ignored

  .globl _start
  .type _start, @function
  .p2align 2
_start:
  li.d $a0, 0
  li.d $a7, 93
  syscall 0
  .size _start, .-_start
