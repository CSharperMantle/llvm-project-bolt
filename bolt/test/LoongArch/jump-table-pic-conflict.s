/// Port of the conservative non-simple jump-table case from
/// X86/jump-table-pic-conflict.s for LoongArch.
///
/// The function has one confirmed PIC jump table and a separate always-unknown
/// indirect jump. The unknown jump rejects the CFG in non-strict mode. BOLT
/// must keep the function non-simple but still rewrite the confirmed table for
/// the emitted function.

// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.o %s
// RUN: ld.lld --emit-relocs -o %t %t.o
// RUN: llvm-bolt --relocs --print-cfg --print-only=known_table_unknown -v=1 \
// RUN:   -o %t.bolt %t 2>&1 | FileCheck --check-prefix=CFG %s
// RUN: llvm-readelf -x .rodata.cold %t.bolt | \
// RUN:   FileCheck --check-prefix=TABLE %s

// CFG: BOLT-WARNING: failed to post-process indirect branches for known_table_unknown
// CFG-LABEL: Binary Function "known_table_unknown" after building cfg
// CFG: IsSimple    : 0
// CFG: jr $a2 # JUMPTABLE @
// CFG: jr $a3 # UNKNOWN CONTROL FLOW
// CFG: PIC Jump table {{.+}} for function known_table_unknown

/// The emitted cases are 12 and 8 bytes before the moved table.
// TABLE: Hex dump of section '.rodata.cold':
// TABLE-NEXT: 0x{{[0-9a-f]+}} f4ffffff f8ffffff

  .text
  .globl _start
  .p2align 2
_start:
  ret
  .size _start, .-_start

  .globl known_table_unknown
  .p2align 2
known_table_unknown:
  beqz   $a0, .Ldispatch
  b      .Lstray
.Ldispatch:
  pcaddi $a1, %pcrel_20(.LJTI)
  slli.d $a0, $a0, 2
  ldx.w  $a2, $a1, $a0
  add.d  $a2, $a1, $a2
  jr     $a2
.Lcase0:
  ret
.Lcase1:
  ret
.Lstray:
  jr     $a3
  .size known_table_unknown, .-known_table_unknown

  .section .rodata,"a",@progbits
  .p2align 2
.LJTI:
  .reloc ., R_LARCH_32_PCREL, .Lcase0
  .4byte 0
  .reloc ., R_LARCH_32_PCREL, .Lcase1 + 4
  .4byte 0
