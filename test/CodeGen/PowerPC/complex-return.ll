; RUN: llc -mcpu=ppc64 < %s | FileCheck %s

target datalayout = "E-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-f128:128:128-v128:128:128-n32:64"
target triple = "powerpc64-unknown-linux-gnu"

define { ppc_fp128, ppc_fp128 } @foo() nounwind {
entry:
  ret { ppc_fp128, ppc_fp128 } { ppc_fp128 0xM400C0000000000000000000000000000, ppc_fp128 0xMC00547AE147AE1483CA47AE147AE147A }
}

; CHECK-LABEL: foo:
; CHECK-DAG: lfd 3
; CHECK-DAG: lfd 4
; CHECK-DAG: lfs 1
; CHECK-DAG: lfs 2

define { float, float } @oof() nounwind {
entry:
  ret { float, float } { float 3.500000e+00, float 0xC00547AE20000000 }
}

; CHECK-LABEL: oof:
; CHECK-DAG: lfs 2
; CHECK-DAG: lfs 1

