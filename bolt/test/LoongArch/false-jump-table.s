/// Port of X86/false-jump-table.s for LoongArch.
///
/// The two predecessors of the dispatch provide different definitions of the
/// table-base register. The last definition in linear order refers to ordinary
/// data, so the optimistic indirect-branch analysis cannot associate .LJTI
/// with the jump. As on X86, symbolization of the valid PC-relative table-base
/// reference must independently register and rewrite .LJTI even though the
/// indirect jump remains unknown and the function becomes non-simple.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --relocs --print-cfg --print-only=false_pic -v=1 \
// RUN:   -o %t.bolt %t 2>&1 | FileCheck --check-prefix=CFG %s
// RUN: llvm-readelf -x .rodata.cold %t.bolt | \
// RUN:   FileCheck --check-prefix=TABLE %s

// CFG: BOLT-WARNING: failed to post-process indirect branches for false_pic
// CFG-LABEL: Binary Function "false_pic" after building cfg
// CFG: IsSimple    : 0
// CFG: pcaddi $a1, %pcrel_20("PG.LJTI/1")
// CFG: pcaddi $a1, %pcrel_20("PG.Lnot_table/1")
// CFG: jr $a2 # UNKNOWN CONTROL FLOW
// CFG: PIC Jump table {{.+}} for function false_pic

// TABLE: Hex dump of section '.rodata.cold':
// TABLE-NEXT: 0x{{[0-9a-f]+}} f8ffffff fcffffff

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

  .globl false_pic
  .p2align 2
false_pic:
  beqz   $a0, .Lfalse_base
  pcaddi $a1, %pcrel_20(.LJTI)
  b      .Ldispatch
.Lfalse_base:
  pcaddi $a1, %pcrel_20(.Lnot_table)
.Ldispatch:
  slli.d $a0, $a0, 2
  ldx.w  $a2, $a1, $a0
  add.d  $a2, $a1, $a2
  jr     $a2
.Lcase0:
  ret
.Lcase1:
  ret
  .size false_pic, .-false_pic

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
