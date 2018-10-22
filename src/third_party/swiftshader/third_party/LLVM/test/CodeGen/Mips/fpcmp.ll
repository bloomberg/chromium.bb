; RUN: llc  < %s -march=mipsel | FileCheck %s -check-prefix=CHECK-MIPS32

@g1 = external global i32

define i32 @f(float %f0, float %f1) nounwind {
entry:
; CHECK-MIPS32: c.olt.s
; CHECK-MIPS32: movt
; CHECK-MIPS32: c.olt.s
; CHECK-MIPS32: movt
  %cmp = fcmp olt float %f0, %f1
  %conv = zext i1 %cmp to i32
  %tmp2 = load i32* @g1, align 4
  %add = add nsw i32 %tmp2, %conv
  store i32 %add, i32* @g1, align 4
  %cond = select i1 %cmp, i32 10, i32 20
  ret i32 %cond
}
