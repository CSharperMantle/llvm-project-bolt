; RUN: opt < %s -passes=tsan -S | FileCheck %s
; RUN: opt < %s -passes=tsan -tsan-use-dominance-analysis=false -S | FileCheck %s --check-prefix=NODOM
; RUN: opt < %s -passes=tsan -tsan-distinguish-volatile -S | FileCheck %s --check-prefix=VOLATILE

; Tests for TSan dominance-based redundant instrumentation elimination.
; Redundant instrumentation is removed when one access (post-)dominates another
; to the same location with no synchronization on any path between them.
;
; Check prefixes:
;   CHECK   - default run (optimization enabled)
;   NODOM   - optimization disabled; all accesses must remain instrumented
;   VOLATILE - -tsan-distinguish-volatile enabled; volatile and non-volatile
;              accesses emit different runtime calls and must not be merged

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"

@g1 = global i32 0, align 4
@g2 = global i32 0, align 4
@arr = global [5 x i32] zeroinitializer, align 4

; Unsafe call (no nosync): blocks dominance-based elimination.
declare void @ext_call()
; nosync but no willreturn/nounwind: safe for dom-elimination, blocks postdom.
declare void @nosync_func() #0
; nosync + willreturn + nounwind: safe for both dom- and postdom-elimination.
declare void @willreturn_func() #1
; Unsafe function returning i32, used in loop tests.
declare i32 @ext_check(...)

; ===========================================================================
; Intra-block dominance
; ===========================================================================

; First write dominates second write to the same location.
define void @intra_block_write_write() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @intra_block_write_write
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       ret void
;
; NODOM-LABEL: define void @intra_block_write_write
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       ret void

; Write dominates following read: write covers read, so read is removed.
define void @intra_block_write_read() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  %val = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @intra_block_write_read
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; First read dominates second read to the same location.
define void @intra_block_read_read() nounwind uwtable sanitize_thread {
entry:
  %v1 = load i32, ptr @g1, align 4
  %v2 = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @intra_block_read_read
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; A dominating read does NOT eliminate a write on a dominated path.
; The read-before-write elimination in chooseInstructionsToInstrument only
; applies within the same basic block, so this uses an inter-block scenario.
; The write is only on one branch so it is NOT the post-dominator of the
; read; the dominance check therefore applies in isolation.
define void @dom_read_does_not_cover_write(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  %v = load i32, ptr @g1, align 4
  br i1 %cond, label %write.path, label %skip
write.path:
  store i32 1, ptr @g1, align 4
  br label %end
skip:
  br label %end
end:
  ret void
}
; CHECK-LABEL: define void @dom_read_does_not_cover_write
; The read dominates the write, but a read cannot cover write-write races.
; Both must remain instrumented.
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK:       write.path:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Path safety
; ===========================================================================

; An unsafe call between two accesses makes the path dirty: no elimination.
define void @path_dirty_call() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  call void @ext_call()
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @path_dirty_call
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; A nosync call does not block dominance elimination.
define void @path_clear_nosync_call() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  call void @nosync_func()
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @path_clear_nosync_call
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Inter-block dominance
; ===========================================================================

; A write in the entry block dominates writes in both branches of a diamond.
define void @inter_block_dom(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  br i1 %cond, label %if.then, label %if.else
if.then:
  store i32 2, ptr @g1, align 4
  br label %if.end
if.else:
  store i32 3, ptr @g1, align 4
  br label %if.end
if.end:
  ret void
}
; CHECK-LABEL: define void @inter_block_dom
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       if.then:
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       if.else:
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       ret void
;
; NODOM-LABEL: define void @inter_block_dom
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       ret void

; ===========================================================================
; Post-dominance
; ===========================================================================

; Write at the merge point post-dominates writes in both branches.
define void @postdom_diamond(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %if.else
if.then:
  store i32 2, ptr @g1, align 4
  br label %if.end
if.else:
  store i32 3, ptr @g1, align 4
  br label %if.end
if.end:
  store i32 4, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_diamond
; CHECK:       if.then:
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       if.else:
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       if.end:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void
;
; NODOM-LABEL: define void @postdom_diamond
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       call void @__tsan_write4(ptr @g1)
; NODOM:       ret void

; Write at the merge point post-dominates reads in both branches.
; Since a write covers both read and write, the reads are eliminated.
define void @postdom_read_covered_by_write(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %if.else
if.then:
  %v1 = load i32, ptr @g1, align 4
  br label %if.end
if.else:
  %v2 = load i32, ptr @g1, align 4
  br label %if.end
if.end:
  store i32 0, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_read_covered_by_write
; CHECK:       if.then:
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       if.else:
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       if.end:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; Read at the merge point post-dominates reads in both branches.
define void @postdom_read_read(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %if.else
if.then:
  %v1 = load i32, ptr @g1, align 4
  br label %if.end
if.else:
  %v2 = load i32, ptr @g1, align 4
  br label %if.end
if.end:
  %v3 = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_read_read
; CHECK:       if.then:
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       if.else:
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       if.end:
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; A read at the merge point post-dominates writes in both branches, but a
; post-dominating read does NOT cover write-write race detection.  Both writes
; must remain instrumented.
define void @postdom_read_does_not_cover_write(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %if.else
if.then:
  store i32 1, ptr @g1, align 4
  br label %if.end
if.else:
  store i32 2, ptr @g1, align 4
  br label %if.end
if.end:
  %v = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_read_does_not_cover_write
; CHECK:       if.then:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       if.else:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       if.end:
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Multi-path: dirty vs clean
; ===========================================================================

; One branch carries an unsafe call: the read at the merge is not eliminated.
define void @multi_path_dirty(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  br i1 %cond, label %then, label %else
then:
  call void @ext_call()
  br label %merge
else:
  call void @nosync_func()
  br label %merge
merge:
  %v = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @multi_path_dirty
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       merge:
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; Both branches carry only nosync calls: the read at the merge is eliminated.
define void @multi_path_clean(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  br i1 %cond, label %then, label %else
then:
  call void @nosync_func()
  br label %merge
else:
  call void @nosync_func()
  br label %merge
merge:
  %v = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @multi_path_clean
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       merge:
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; Adjacent stores in the same block are eliminated; a dirty path to the merge
; block keeps the final store instrumented.
define void @mixed_intra_inter(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  store i32 2, ptr @g1, align 4
  br i1 %cond, label %dirty, label %clean
dirty:
  call void @ext_call()
  br label %merge
clean:
  call void @nosync_func()
  br label %merge
merge:
  store i32 3, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @mixed_intra_inter
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       merge:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Path safety: dirty suffix / prefix / unrelated path
; ===========================================================================

; Unsafe call in suffix of start block blocks postdom elimination.
define void @postdom_dirty_start_suffix(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  call void @ext_call()
  br i1 %cond, label %path.then, label %path.else
path.then:
  br label %merge
path.else:
  br label %merge
merge:
  %v = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_dirty_start_suffix
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       merge:
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; Unsafe call in prefix of end block blocks dom elimination.
define void @postdom_dirty_end_prefix() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  br label %end
end:
  call void @ext_call()
  %v = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_dirty_end_prefix
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; A dirty path that does not reach the end block must not block elimination.
define void @postdom_dirty_unrelated_path(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  br i1 %cond, label %to.end, label %to.dead
to.end:
  br label %end
to.dead:
  call void @ext_call()
  br label %dead
dead:
  ret void
end:
  %v = load i32, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_dirty_unrelated_path
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       end:
; CHECK-NOT:   call void @__tsan_read4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Loops
; ===========================================================================

; Loop with an unsafe call in the condition: dom-safety fails, both stores
; remain instrumented.
define void @postdom_loop() nounwind uwtable sanitize_thread {
entry:
  br label %while.cond
while.cond:
  %v = call i32 (...) @ext_check()
  %tobool = icmp ne i32 %v, 0
  br i1 %tobool, label %while.body, label %while.end
while.body:
  store i32 1, ptr @g1, align 4
  br label %while.cond
while.end:
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_loop
; CHECK:       while.body:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       while.end:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; Pure loop (only branch instructions, no calls or atomics): the store inside
; the loop body is not dominated by the store after the loop (while.body does
; not dominate while.end), and LLVM's PDT does not establish post-dominance
; across a potentially-infinite loop.  Both stores must remain instrumented.
define void @postdom_pure_loop(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br label %while.cond
while.cond:
  br i1 %cond, label %while.body, label %while.end
while.body:
  store i32 1, ptr @g1, align 4
  br label %while.cond
while.end:
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_pure_loop
; CHECK:       while.body:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       while.end:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Post-dominance: call-attribute safety
; ===========================================================================

; A nosync call without willreturn is safe for dom-elimination, but blocks
; postdom-elimination (may not return, so the postdominator might never execute).
define void @postdom_blocked_nosync_call(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %if.else
if.then:
  store i32 1, ptr @g1, align 4
  call void @nosync_func()
  br label %if.end
if.else:
  br label %if.end
if.end:
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_blocked_nosync_call
; CHECK:       if.then:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       if.end:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; A nosync call (safe for dom, unsafe for postdom) in a separate INTERMEDIATE
; block between CurrInst's block and DomInst's block must block postdom
; elimination via the cone traversal (not the local suffix/prefix checks).
; This directly exercises the intermediate-block safety check in
; traverseReachableAndCheckSafety.
define void @postdom_blocked_nosync_call_intermediate(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %middle
if.then:
  store i32 1, ptr @g1, align 4
  br label %middle
middle:
  call void @nosync_func()
  br label %exit
exit:
  store i32 2, ptr @g1, align 4
  ret void
}
; The nosync call in %middle is not in the suffix of %if.then or in the prefix
; of %exit, so only the cone traversal can detect it.  Both stores must remain.
; CHECK-LABEL: define void @postdom_blocked_nosync_call_intermediate
; CHECK:       if.then:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       exit:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; A nosync + willreturn + nounwind call is safe for postdom-elimination.
define void @postdom_allowed_willreturn_call(i1 %cond) nounwind uwtable sanitize_thread {
entry:
  br i1 %cond, label %if.then, label %if.else
if.then:
  store i32 1, ptr @g1, align 4
  call void @willreturn_func()
  br label %if.end
if.else:
  br label %if.end
if.end:
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @postdom_allowed_willreturn_call
; CHECK:       if.then:
; CHECK-NOT:   call void @__tsan_write4(ptr @g1)
; CHECK:       if.end:
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Synchronization barriers
; ===========================================================================

; An atomic RMW operation is a synchronization point: no elimination.
define void @atomic_blocks_dom_elim() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  %old = atomicrmw add ptr @g1, i32 1 seq_cst
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @atomic_blocks_dom_elim
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; A seq_cst fence is a synchronization point: no elimination.
define void @fence_blocks_dom_elim() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  fence seq_cst
  store i32 2, ptr @g1, align 4
  ret void
}
; CHECK-LABEL: define void @fence_blocks_dom_elim
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       ret void

; ===========================================================================
; Alias analysis
; ===========================================================================

; Accesses to distinct globals are NoAlias: no elimination.
define void @no_alias() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  store i32 2, ptr @g2, align 4
  ret void
}
; CHECK-LABEL: define void @no_alias
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_write4(ptr @g2)
; CHECK:       ret void

; Zero-index GEP is MustAlias with the base: elimination fires.
define void @mustalias_gep0() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  %p = getelementptr i32, ptr @g1, i64 0
  %v = load i32, ptr %p, align 4
  ret void
}
; CHECK-LABEL: define void @mustalias_gep0
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK-NOT:   call void @__tsan_read4(
; CHECK:       ret void

; Different GEP offsets into the same array are NoAlias: no elimination.
define void @noalias_different_offsets() nounwind uwtable sanitize_thread {
entry:
  %p0 = getelementptr [5 x i32], ptr @arr, i64 0, i64 0
  %p1 = getelementptr [5 x i32], ptr @arr, i64 0, i64 1
  store i32 1, ptr %p0, align 4
  store i32 2, ptr %p1, align 4
  ret void
}
; CHECK-LABEL: define void @noalias_different_offsets
; CHECK:       call void @__tsan_write4(
; CHECK:       call void @__tsan_write4(
; CHECK:       ret void

; Identical GEP offsets into the same array are MustAlias: elimination fires.
define void @mustalias_same_offsets() nounwind uwtable sanitize_thread {
entry:
  %p0 = getelementptr [5 x i32], ptr @arr, i64 0, i64 1
  %p1 = getelementptr [5 x i32], ptr @arr, i64 0, i64 1
  store i32 1, ptr %p0, align 4
  store i32 2, ptr %p1, align 4
  ret void
}
; CHECK-LABEL: define void @mustalias_same_offsets
; CHECK:       call void @__tsan_write4(
; CHECK-NOT:   call void @__tsan_write4(
; CHECK:       ret void

; phi selecting between two globals is MayAlias: no elimination.
define void @mayalias_phi(i1 %c) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  br i1 %c, label %A, label %B
A:
  br label %join
B:
  br label %join
join:
  %p = phi ptr [ @g1, %A ], [ @g2, %B ]
  %v = load i32, ptr %p, align 4
  ret void
}
; CHECK-LABEL: define void @mayalias_phi
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_read4(
; CHECK:       ret void

; ptrtoint/inttoptr round-trip breaks MustAlias: no elimination.
define void @noalias_ptr_roundtrip() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  %i  = ptrtoint ptr @g1 to i64
  %p2 = inttoptr i64 %i to ptr
  %v  = load i32, ptr %p2, align 4
  ret void
}
; CHECK-LABEL: define void @noalias_ptr_roundtrip
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK:       call void @__tsan_read4(
; CHECK:       ret void

; select with identical arms is MustAlias: elimination fires.
define void @mustalias_select_same_ptr(i1 %c) nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  %p = select i1 %c, ptr @g1, ptr @g1
  %v = load i32, ptr %p, align 4
  ret void
}
; CHECK-LABEL: define void @mustalias_select_same_ptr
; CHECK:       call void @__tsan_write4(ptr @g1)
; CHECK-NOT:   call void @__tsan_read4(
; CHECK:       ret void

; ===========================================================================
; Volatile (VOLATILE check prefix only)
;
; When -tsan-distinguish-volatile is active, volatile and non-volatile accesses
; to the same address emit different TSan calls and must never be merged.
; ===========================================================================

; Non-volatile write followed by volatile write: both must be kept.
define void @write_then_volatile_write() nounwind uwtable sanitize_thread {
entry:
  store i32 1, ptr @g1, align 4
  store volatile i32 2, ptr @g1, align 4
  ret void
}
; VOLATILE-LABEL: define void @write_then_volatile_write
; VOLATILE:       call void @__tsan_write4(ptr @g1)
; VOLATILE:       call void @__tsan_volatile_write4(ptr @g1)
; VOLATILE:       ret void

; Volatile write followed by non-volatile write: both must be kept.
define void @volatile_write_then_write() nounwind uwtable sanitize_thread {
entry:
  store volatile i32 1, ptr @g1, align 4
  store i32 2, ptr @g1, align 4
  ret void
}
; VOLATILE-LABEL: define void @volatile_write_then_write
; VOLATILE:       call void @__tsan_volatile_write4(ptr @g1)
; VOLATILE:       call void @__tsan_write4(ptr @g1)
; VOLATILE:       ret void

attributes #0 = { nosync }
attributes #1 = { nosync willreturn nounwind }
