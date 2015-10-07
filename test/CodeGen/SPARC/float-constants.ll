; RUN: llc < %s -march=sparc | FileCheck %s

;; Bitcast should not do a runtime conversion, but rather emit a
;; constant into integer registers directly.
; CHECK-LABEL: bitcast:
; CHECK: sethi 1049856, %o0
; CHECK: sethi 0, %o1
define <2 x i32> @bitcast() {
  %1 = bitcast i64 5 to <2 x i32>
  ret <2 x i32> %1
}

;; Same thing for a call using a double (which gets passed in integer
;; registers)
; CHECK-LABEL: test_call
; CHECK: sethi 1049856, %o0
; CHECK: sethi 0, %o1
;declare void @a(double)
;define void @test_call() {
;  call void @a(double 5.0)
;  ret void
;}

;; And for a libcall emitted from the pow intrinsic.  (libcall
;; emission happens after SelectionDAG type legalization, and was thus
;; crashing due to an earlier workaround for this issue)

; CHECK-LABEL: test_intrins_call
; CHECK: sethi 1049856, %o0
; CHECK: sethi 0, %o1
;declare double @llvm.pow.f64(double, double)
;define double @test_intrins_call() {
;  %1 = call double @llvm.pow.f64(double 2.0, double 2.0)
;  ret double %1
;}
