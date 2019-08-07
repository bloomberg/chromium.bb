; RUN: opt < %s -instcombine -S | FileCheck %s
target datalayout = "E-p:64:64:64-a0:0:8-f32:32:32-f64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-v64:64:64-v128:128:128"

; Instcombine should be able to prove vector alignment in the
; presence of a few mild address computation tricks.

; CHECK: @test0(
; CHECK: align 16

define void @test0(i8* %b, i64 %n, i64 %u, i64 %y) nounwind  {
entry:
  %c = ptrtoint i8* %b to i64
  %d = and i64 %c, -16
  %e = inttoptr i64 %d to double*
  %v = mul i64 %u, 2
  %z = and i64 %y, -2
  %t1421 = icmp eq i64 %n, 0
  br i1 %t1421, label %return, label %bb

bb:
  %i = phi i64 [ %indvar.next, %bb ], [ 20, %entry ]
  %j = mul i64 %i, %v
  %h = add i64 %j, %z
  %t8 = getelementptr double* %e, i64 %h
  %p = bitcast double* %t8 to <2 x double>*
  store <2 x double><double 0.0, double 0.0>, <2 x double>* %p, align 8
  %indvar.next = add i64 %i, 1
  %exitcond = icmp eq i64 %indvar.next, %n
  br i1 %exitcond, label %return, label %bb

return:
  ret void
}

; When we see a unaligned load from an insufficiently aligned global or
; alloca, increase the alignment of the load, turning it into an aligned load.

; CHECK: @test1(
; CHECK: tmp = load
; CHECK: GLOBAL{{.*}}align 16

@GLOBAL = internal global [4 x i32] zeroinitializer

define <16 x i8> @test1(<2 x i64> %x) {
entry:
	%tmp = load <16 x i8>* bitcast ([4 x i32]* @GLOBAL to <16 x i8>*), align 1
	ret <16 x i8> %tmp
}

; When a load or store lacks an explicit alignment, add one.

; CHECK: @test2(
; CHECK: load double* %p, align 8
; CHECK: store double %n, double* %p, align 8

define double @test2(double* %p, double %n) nounwind {
  %t = load double* %p
  store double %n, double* %p
  ret double %t
}
