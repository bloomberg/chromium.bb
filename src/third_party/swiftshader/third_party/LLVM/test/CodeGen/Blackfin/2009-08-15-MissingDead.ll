; RUN: llc < %s -march=bfin -verify-machineinstrs

; LocalRewriter can forget to transfer a <def,dead> flag when setting up call
; argument registers. This then causes register scavenger asserts.

declare i32 @printf(i8*, i32, float)

define i32 @testissue(i32 %i, float %x, float %y) {
  br label %bb1

bb1:                                              ; preds = %bb1, %0
  %x2 = fmul float %x, 5.000000e-01               ; <float> [#uses=1]
  %y2 = fmul float %y, 0x3FECCCCCC0000000         ; <float> [#uses=1]
  %z2 = fadd float %x2, %y2                       ; <float> [#uses=1]
  %z3 = fadd float undef, %z2                     ; <float> [#uses=1]
  %i1 = shl i32 %i, 3                             ; <i32> [#uses=1]
  %j1 = add i32 %i, 7                             ; <i32> [#uses=1]
  %m1 = add i32 %i1, %j1                          ; <i32> [#uses=2]
  %b = icmp sle i32 %m1, 6                        ; <i1> [#uses=1]
  br i1 %b, label %bb1, label %bb2

bb2:                                              ; preds = %bb1
  %1 = call i32 @printf(i8* undef, i32 %m1, float %z3); <i32> [#uses=0]
  ret i32 0
}
