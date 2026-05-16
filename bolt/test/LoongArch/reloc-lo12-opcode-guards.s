// REQUIRES: asserts
// RUN: llvm-mc --triple=loongarch64 --filetype=obj -o %t.exec.o %s
// RUN: ld.lld --emit-relocs -o %t.exec %t.exec.o
// RUN: llvm-readelf -rW %t.exec | FileCheck --check-prefix=RELOC-EXEC %s
// RUN: llvm-bolt --debug-only=bolt-symbolizer --print-cfg --print-only=_start -o %t.exec.bolt %t.exec 2>&1 | FileCheck --check-prefix=EXEC %s
// RUN: llvm-mc --triple=loongarch64 --defsym=BUILD_TLS=1 --filetype=obj -o %t.tls.o %s
// RUN: ld.lld -shared --emit-relocs -o %t.tls %t.tls.o
// RUN: llvm-readelf -rW %t.tls | FileCheck --check-prefix=RELOC-TLS %s
// RUN: llvm-bolt --debug-only=bolt-symbolizer --print-cfg --print-only=_start -o %t.tls.bolt %t.tls 2>&1 | FileCheck --check-prefix=TLS %s

// RELOC-EXEC: R_LARCH_PCALA_HI20
// RELOC-EXEC: R_LARCH_PCALA_LO12
// RELOC-EXEC: R_LARCH_PCADD_HI20
// RELOC-EXEC: R_LARCH_PCADD_LO12
// RELOC-EXEC: R_LARCH_PCALA_HI20
// RELOC-EXEC: R_LARCH_PCALA_LO12

// EXEC: BOLT-INFO: enabling relocation mode
// EXEC-NOT: BOLT-DEBUG: ignoring relocation
// EXEC-NOT: BOLT-WARNING: Failed to analyze
// EXEC-LABEL: Binary Function "_start
// EXEC:       st.d $a1, $a0
// EXEC:       pcaddu12i $a2, %pcadd_hi20(data)
// EXEC:       st.d $a1, $a2

// RELOC-TLS: R_LARCH_TLS_IE_PC_HI20
// RELOC-TLS: R_LARCH_TLS_IE_PC_LO12
// RELOC-TLS: R_LARCH_TLS_DESC_PC_HI20
// RELOC-TLS: R_LARCH_TLS_DESC_PC_LO12

// TLS: BOLT-INFO: enabling relocation mode
// TLS-NOT: BOLT-DEBUG: ignoring relocation
// TLS-NOT: BOLT-WARNING: Failed to analyze
// TLS-LABEL: Binary Function "_start
// TLS:       addi.d $a3, $a3
// TLS:       st.d $a1, $a4

  .text
  .globl _start
  .p2align 2
_start:
.ifdef BUILD_TLS
  pcalau12i $a3, %ie_pc_hi20(tdata)
  addi.d $a3, $a3, %ie_pc_lo12(tdata)
  pcalau12i $a4, %desc_pc_hi20(tdata)
  st.d $a1, $a4, %desc_pc_lo12(tdata)
  ret
  .size _start, .-_start

  .section .tbss,"awT",@nobits
  .globl tdata
  .p2align 3
tdata:
  .quad 0
  .size tdata, .-tdata
.else
  pcalau12i $a0, %pc_hi20(data)
  st.d $a1, $a0, %pc_lo12(data)
.Lpcadd:
  pcaddu12i $a2, %pcadd_hi20(data)
  st.d $a1, $a2, %pcadd_lo12(.Lpcadd)
  pcalau12i $a4, %pc_hi20(target)
  jirl $zero, $a4, %pc_lo12(target)
target:
  ret
  .size _start, .-_start

  .data
  .globl data
  .p2align 3
data:
  .quad 0
  .size data, .-data
.endif
