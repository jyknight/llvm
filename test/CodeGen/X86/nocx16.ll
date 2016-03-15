; RUN: llc < %s -march=x86-64 -mcpu=corei7 -mattr=-cx16 | FileCheck %s
define void @test(i128* %a) nounwind {
entry:
; CHECK: __atomic_compare_exchange_16
  %0 = cmpxchg i128* %a, i128 1, i128 1 seq_cst seq_cst
; CHECK: __atomic_exchange_16
  %1 = atomicrmw xchg i128* %a, i128 1 seq_cst
; CHECK: __atomic_fetch_add_16
  %2 = atomicrmw add i128* %a, i128 1 seq_cst
; CHECK: __atomic_fetch_sub_16
  %3 = atomicrmw sub i128* %a, i128 1 seq_cst
; CHECK: __atomic_fetch_and_16
  %4 = atomicrmw and i128* %a, i128 1 seq_cst
; CHECK: __atomic_fetch_nand_16
  %5 = atomicrmw nand i128* %a, i128 1 seq_cst
; CHECK: __atomic_fetch_or_16
  %6 = atomicrmw or i128* %a, i128 1 seq_cst
; CHECK: __atomic_fetch_xor_16
  %7 = atomicrmw xor i128* %a, i128 1 seq_cst
  ret void
}
