; RUN: llc -march=mips < %s | FileCheck %s

define void @f1(i64 %ll1, float %f, i64 %ll, i32 %i, float %f2) nounwind {
entry:
; CHECK: addu $[[R1:[0-9]+]], $zero, $5
; CHECK: addu $[[R0:[0-9]+]], $zero, $4
; CHECK: lw  $25, %call16(ff1)
; CHECK: ori $6, ${{[0-9]+}}, 3855
; CHECK: ori $7, ${{[0-9]+}}, 22136
; CHECK: jalr
  tail call void @ff1(i32 %i, i64 1085102592623924856) nounwind
; CHECK: lw $25, %call16(ff2)
; CHECK: lw $[[R2:[0-9]+]], 88($sp)
; CHECK: lw $[[R3:[0-9]+]], 92($sp)
; CHECK: addu $4, $zero, $[[R2]]
; CHECK: addu $5, $zero, $[[R3]]
; CHECK: jalr $25
  tail call void @ff2(i64 %ll, double 3.000000e+00) nounwind
  %sub = add nsw i32 %i, -1
; CHECK: sw $[[R0]], 24($sp)
; CHECK: sw $[[R1]], 28($sp)
; CHECK: lw $25, %call16(ff3)
; CHECK: addu $6, $zero, $[[R2]]
; CHECK: addu $7, $zero, $[[R3]]
; CHECK: jalr $25
  tail call void @ff3(i32 %i, i64 %ll, i32 %sub, i64 %ll1) nounwind
  ret void
}

declare void @ff1(i32, i64)

declare void @ff2(i64, double)

declare void @ff3(i32, i64, i32, i64)
