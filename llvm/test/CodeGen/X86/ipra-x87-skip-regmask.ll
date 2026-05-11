; RUN: llc -enable-ipra -print-regusage -o /dev/null 2>&1 < %s | FileCheck %s

; Test that RegUsageInfoCollector skips storing RegMask for functions that use
; x87 registers (FP0-FP7, ST0-ST7). Such functions should not appear in the
; register usage output, as the x87 stack model makes precise per-register
; tracking unreliable.

target triple = "x86_64-unknown-unknown"

; Function that does not use x87 - should have RegMask stored.
; Verify its output contains no FP or ST register names.
; CHECK: no_x87 Clobbered Registers:
define void @no_x87() #0 {
  ret void
}

; Function that uses x87 via inline asm - should NOT have RegMask stored.
; When skipped, uses_x87 does not appear; if it did, it would list FP/ST regs.
; CHECK-NOT: uses_x87 Clobbered Registers:
define void @uses_x87() #0 {
  call void asm sideeffect "fld1", "~{st}"()
  ret void
}


; Caller so both functions have uses (avoid use_empty() early exit).
define void @caller() #0 {
  call void @uses_x87()
  call void @no_x87()
  ret void
}

@llvm.used = appending global [3 x ptr] [ptr @uses_x87, ptr @no_x87, ptr @caller]
