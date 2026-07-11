/// Check that LongJmp accounts for an indexed function that becomes ignored
/// after the output function list is populated. Otherwise the hot/cold
/// boundary is delayed and the caller is laid out before the split cold
/// fragment that precedes it in the emitted .text.cold section.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld -m elf64loongarch --emit-relocs -e caller -o %t %t.o
// RUN: link_fdata --no-lbr %s %t %t.fdata
// RUN: llvm-bolt %t -o %t.bolt --data=%t.fdata \
// RUN:   --reorder-functions=exec-count --split-functions --force-patch \
// RUN:   --skip-funcs=old_target --align-text=0x8000000 \
// RUN:   --print-longjmp --print-only=caller 2>&1 | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.bolt | FileCheck --check-prefix=OBJDUMP %s

// CHECK: BOLT-WARNING: failed to patch entries in late_ignored
// CHECK: BOLT-INFO: Starting stub-insertion pass
// CHECK-LABEL: Binary Function "caller" after long-jmp
// CHECK:      bl .LStub{{[0-9]+}}
// CHECK-NEXT: ret
// CHECK:      pcaddu18i $r21, %call36(old_target)
// CHECK-NEXT: jr $r21
// CHECK-NOT: BOLT-ERROR

// OBJDUMP:       <caller>:
// OBJDUMP-NEXT:    bl 8 <caller+0x8>
// OBJDUMP-NEXT:    ret
// OBJDUMP-NEXT:    pcaddu18i $r21,
// OBJDUMP-NEXT:    jirl $zero, $r21,

/// Give these two functions valid indices. late_ignored is still present in
/// the output function list when PatchEntries later marks it ignored.
// FDATA: 1 hot_emitted #hot_emitted# 100
// FDATA: 1 late_ignored #late_ignored# 50

  .text

/// Keep the call target in the original text so the final cold call is near the
/// negative B26 boundary.
  .globl old_target
  .type old_target, @function
old_target:
  ret
  .size old_target, .-old_target

/// The large unprofiled successor is split into .text.cold before caller.
  .globl hot_emitted
  .type hot_emitted, @function
hot_emitted:
  bnez $a0, .Lcold
  ret
.Lcold:
  .rept 34816
  addi.d $a1, $a1, 1
  .endr
  ret
  .size hot_emitted, .-hot_emitted

/// A LoongArch entry patch is two instructions, so this adjacent one-
/// instruction function becomes ignored by PatchEntries under --force-patch.
  .globl late_ignored
  .type late_ignored, @function
late_ignored:
  ret
  .size late_ignored, .-late_ignored

  .globl caller
  .type caller, @function
caller:
  bl old_target
  ret
  .size caller, .-caller

  .reloc 0, R_LARCH_NONE
