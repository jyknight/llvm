; RUN: llc < %s -march=xcore | FileCheck %s

; CHECK-LABEL: atomic_fence
; CHECK: #MEMBARRIER
; CHECK: #MEMBARRIER
; CHECK: #MEMBARRIER
; CHECK: #MEMBARRIER
; CHECK: retsp 0
define void @atomic_fence() nounwind {
entry:
  fence acquire
  fence release
  fence acq_rel
  fence seq_cst
  ret void
}

@pool = external global i64

define void @atomicloadstore() nounwind {
entry:
; CHECK-LABEL: atomicloadstore

; CHECK: bl __atomic_load_4
  %0 = load atomic i32, i32* bitcast (i64* @pool to i32*) acquire, align 4

; CHECK: bl __atomic_store_2
  store atomic i16 5, i16* bitcast (i64* @pool to i16*) release, align 2

  ret void
}
