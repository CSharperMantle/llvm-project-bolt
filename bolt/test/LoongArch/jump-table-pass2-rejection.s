/// Check the LoongArch counterpart of X86's Pass 2 jump-table rejection.
///
/// Pass 1 sees the last linear definition of $a1 before the dispatch and
/// tentatively recognizes .LJTI as a PIC jump table. Once the CFG exists,
/// Pass 2 sees that the two predecessors provide different definitions of
/// $a1 and rejects the dispatch CFG. In non-strict mode the function must be
/// non-simple, while the table recognized in Pass 1 remains attached and is
/// rewritten for the emitted function.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --relocs --print-cfg --print-only=rejected_pic -v=1 \
// RUN:   -o %t.bolt %t 2>&1 | FileCheck --check-prefix=CFG %s
// RUN: llvm-readelf -x .rodata.cold %t.bolt | \
// RUN:   FileCheck --check-prefix=TABLE %s

// CFG: BOLT-WARNING: failed to post-process indirect branches for rejected_pic
// CFG-LABEL: Binary Function "rejected_pic" after building cfg
// CFG: IsSimple    : 0
// CFG: jr $a2 # JUMPTABLE @
// CFG: PIC Jump table {{.+}} for function rejected_pic

/// The emitted cases immediately precede the moved table. These are -8 and -4
/// relative to the new table address, rather than offsets to .bolt.org.text.
// TABLE: Hex dump of section '.rodata.cold':
// TABLE-NEXT: 0x{{[0-9a-f]+}} f8ffffff fcffffff

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

  .globl rejected_pic
  .p2align 2
rejected_pic:
  beqz   $a0, .Lreal_base
  pcaddi $a1, %pcrel_20(.Lnot_table)
  b      .Ldispatch
.Lreal_base:
  pcaddi $a1, %pcrel_20(.LJTI)
.Ldispatch:
  slli.d $a0, $a0, 2
  ldx.w  $a2, $a1, $a0
  add.d  $a2, $a1, $a2
  jr     $a2
.Lcase0:
  ret
.Lcase1:
  ret
  .size rejected_pic, .-rejected_pic

  .section .rodata,"a",@progbits
  .p2align 2
.LJTI:
  .reloc ., R_LARCH_32_PCREL, .Lcase0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Lcase1 + 4
  .4byte 0
.Lnot_table:
  .4byte 0
  .4byte 0
