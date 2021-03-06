; RUN: opt < %s -instcombine -S | FileCheck %s

declare <4 x float> @llvm.x86.vcvtph2ps.128(<8 x i16>)
declare <8 x float> @llvm.x86.vcvtph2ps.256(<8 x i16>)

;
; Vector Demanded Bits
;

; Only bottom 4 elements required.
define <4 x float> @demand_vcvtph2ps_128(<8 x i16> %A) {
; CHECK-LABEL: @demand_vcvtph2ps_128
; CHECK-NEXT: %1 = tail call <4 x float> @llvm.x86.vcvtph2ps.128(<8 x i16> %A)
; CHECK-NEXT: ret <4 x float> %1
  %1 = shufflevector <8 x i16> %A, <8 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 0, i32 1, i32 2, i32 3>
  %2 = tail call <4 x float> @llvm.x86.vcvtph2ps.128(<8 x i16> %1)
  ret <4 x float> %2
}

; All 8 elements required.
define <8 x float> @demand_vcvtph2ps_256(<8 x i16> %A) {
; CHECK-LABEL: @demand_vcvtph2ps_256
; CHECK-NEXT: %1 = shufflevector <8 x i16> %A, <8 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 0, i32 1, i32 2, i32 3>
; CHECK-NEXT: %2 = tail call <8 x float> @llvm.x86.vcvtph2ps.256(<8 x i16> %1)
; CHECK-NEXT: ret <8 x float> %2
  %1 = shufflevector <8 x i16> %A, <8 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 0, i32 1, i32 2, i32 3>
  %2 = tail call <8 x float> @llvm.x86.vcvtph2ps.256(<8 x i16> %1)
  ret <8 x float> %2
}

;
; Constant Folding
;

define <4 x float> @fold_vcvtph2ps_128() {
; CHECK-LABEL: @fold_vcvtph2ps_128
; CHECK-NEXT: ret <4 x float> <float 0.000000e+00, float 5.000000e-01, float 1.000000e+00, float -0.000000e+00>
  %1 = tail call <4 x float> @llvm.x86.vcvtph2ps.128(<8 x i16> <i16 0, i16 14336, i16 15360, i16 32768, i16 16384, i16 31743, i16 48128, i16 49152>)
  ret <4 x float> %1
}

define <8 x float> @fold_vcvtph2ps_256() {
; CHECK-LABEL: @fold_vcvtph2ps_256
; CHECK-NEXT: ret <8 x float> <float 0.000000e+00, float 5.000000e-01, float 1.000000e+00, float -0.000000e+00, float 2.000000e+00, float 6.550400e+04, float -1.000000e+00, float -2.000000e+00>
  %1 = tail call <8 x float> @llvm.x86.vcvtph2ps.256(<8 x i16> <i16 0, i16 14336, i16 15360, i16 32768, i16 16384, i16 31743, i16 48128, i16 49152>)
  ret <8 x float> %1
}

define <4 x float> @fold_vcvtph2ps_128_zero() {
; CHECK-LABEL: @fold_vcvtph2ps_128_zero
; CHECK-NEXT: ret <4 x float> zeroinitializer
  %1 = tail call <4 x float> @llvm.x86.vcvtph2ps.128(<8 x i16> <i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0>)
  ret <4 x float> %1
}

define <8 x float> @fold_vcvtph2ps_256_zero() {
; CHECK-LABEL: @fold_vcvtph2ps_256_zero
; CHECK-NEXT: ret <8 x float> zeroinitializer
  %1 = tail call <8 x float> @llvm.x86.vcvtph2ps.256(<8 x i16> <i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0>)
  ret <8 x float> %1
}
