; RUN: llc < %s -march=arm64 | FileCheck %s

define void @t1(i8* nocapture %c) nounwind optsize {
entry:
; CHECK-LABEL: t1:
; CHECK: str wzr, [x0, #8]
; CHECK: str xzr, [x0]
  call void @llvm.memset.p0i8.i64(i8* %c, i8 0, i64 12, i32 8, i1 false)
  ret void
}

;; FIXME: this code got worse, as it now knows that it can merge the
;; store to sp+8 and sp+16 into a 16byte store. (good!). But,
;; unfortunately, the backend doesn't know how to emit the obvious:
;;   stp xzr,xzr, [sp, #8]
;; instruction for a 16-byte store of a constant zero...

define void @t2() nounwind ssp {
entry:
; CHECK-LABEL: t2:
; CHECK: movi    v0.2d, #0000000000000000
; CHECK: strh wzr, [sp, #32]
; CHECK: str xzr, [sp, #24]
; CHECK: stur q0, [sp, #8]

  %buf = alloca [26 x i8], align 1
  %0 = getelementptr inbounds [26 x i8], [26 x i8]* %buf, i32 0, i32 0
  call void @llvm.memset.p0i8.i32(i8* %0, i8 0, i32 26, i32 1, i1 false)
  call void @something(i8* %0) nounwind
  ret void
}

declare void @something(i8*) nounwind
declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1) nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture, i8, i64, i32, i1) nounwind
