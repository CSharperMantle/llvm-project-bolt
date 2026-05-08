; RUN: llc < %s -mtriple=bpfel | FileCheck %s
; RUN: llc < %s -mtriple=bpfeb | FileCheck %s

; CHECK-LABEL: atomic_fence:
; CHECK-COUNT-7: #MEMBARRIER
; CHECK-NEXT:    [[R:r[0-9]+]] = 0
; CHECK-NEXT:    lock *(u64 *)(r10 - {{[0-9]+}}) += [[R]]
; CHECK-NEXT:    exit
define void @atomic_fence() nounwind {
entry:
  fence acquire
  fence release
  fence acq_rel
  fence syncscope("singlethread") acquire
  fence syncscope("singlethread") release
  fence syncscope("singlethread") acq_rel
  fence syncscope("singlethread") seq_cst
  fence seq_cst
  ret void
}
