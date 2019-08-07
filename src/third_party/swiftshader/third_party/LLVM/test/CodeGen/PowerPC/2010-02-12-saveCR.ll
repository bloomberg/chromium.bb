; RUN: llc < %s -mtriple=powerpc-apple-darwin | FileCheck %s
; ModuleID = 'hh.c'
target datalayout = "E-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f128:64:128-n32"
target triple = "powerpc-apple-darwin9.6"
; This formerly used R0 for both the stack address and CR.

define void @foo() nounwind {
entry:
;CHECK:  mfcr r2
;CHECK:  rlwinm r2, r2, 8, 0, 31
;CHECK:  lis r0, 1
;CHECK:  ori r0, r0, 34540
;CHECK:  stwx r2, r1, r0
  %x = alloca [100000 x i8]                       ; <[100000 x i8]*> [#uses=1]
  %"alloca point" = bitcast i32 0 to i32          ; <i32> [#uses=0]
  %x1 = bitcast [100000 x i8]* %x to i8*          ; <i8*> [#uses=1]
  call void @bar(i8* %x1) nounwind
  call void asm sideeffect "", "~{cr2}"() nounwind
  br label %return

return:                                           ; preds = %entry
;CHECK:  lis r0, 1
;CHECK:  ori r0, r0, 34540
;CHECK:  lwzx r2, r1, r0
;CHECK:  rlwinm r2, r2, 24, 0, 31
;CHECK:  mtcrf 32, r2
  ret void
}

declare void @bar(i8*)
