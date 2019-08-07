; RUN: opt < %s -basicaa -memcpyopt -dse -S | FileCheck %s

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128"
target triple = "i686-apple-darwin9"

%0 = type { x86_fp80, x86_fp80 }
%1 = type { i32, i32 }

define void @test1(%0* sret  %agg.result, x86_fp80 %z.0, x86_fp80 %z.1) nounwind  {
entry:
  %tmp2 = alloca %0
  %memtmp = alloca %0, align 16
  %tmp5 = fsub x86_fp80 0xK80000000000000000000, %z.1
  call void @ccoshl(%0* sret %memtmp, x86_fp80 %tmp5, x86_fp80 %z.0) nounwind
  %tmp219 = bitcast %0* %tmp2 to i8*
  %memtmp20 = bitcast %0* %memtmp to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %tmp219, i8* %memtmp20, i32 32, i32 16, i1 false)
  %agg.result21 = bitcast %0* %agg.result to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %agg.result21, i8* %tmp219, i32 32, i32 16, i1 false)
  ret void

; Check that one of the memcpy's are removed.
;; FIXME: PR 8643 We should be able to eliminate the last memcpy here.

; CHECK: @test1
; CHECK: call void @ccoshl
; CHECK: call void @llvm.memcpy
; CHECK-NOT: llvm.memcpy
; CHECK: ret void
}

declare void @ccoshl(%0* sret , x86_fp80, x86_fp80) nounwind 


; The intermediate alloca and one of the memcpy's should be eliminated, the
; other should be related with a memmove.
define void @test2(i8* %P, i8* %Q) nounwind  {
  %memtmp = alloca %0, align 16
  %R = bitcast %0* %memtmp to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %R, i8* %P, i32 32, i32 16, i1 false)
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %Q, i8* %R, i32 32, i32 16, i1 false)
  ret void
        
; CHECK: @test2
; CHECK-NEXT: call void @llvm.memmove{{.*}}(i8* %Q, i8* %P
; CHECK-NEXT: ret void
}




@x = external global %0

define void @test3(%0* noalias sret %agg.result) nounwind  {
  %x.0 = alloca %0
  %x.01 = bitcast %0* %x.0 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %x.01, i8* bitcast (%0* @x to i8*), i32 32, i32 16, i1 false)
  %agg.result2 = bitcast %0* %agg.result to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %agg.result2, i8* %x.01, i32 32, i32 16, i1 false)
  ret void
; CHECK: @test3
; CHECK-NEXT: %agg.result2 = bitcast 
; CHECK-NEXT: call void @llvm.memcpy
; CHECK-NEXT: ret void
}


; PR8644
define void @test4(i8 *%P) {
  %A = alloca %1
  %a = bitcast %1* %A to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %a, i8* %P, i64 8, i32 4, i1 false)
  call void @test4a(i8* byval align 1 %a)
  ret void
; CHECK: @test4
; CHECK-NEXT: call void @test4a(
}

declare void @test4a(i8* byval align 1)
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture, i64, i32, i1) nounwind

%struct.S = type { i128, [4 x i8]}

@sS = external global %struct.S, align 16

declare void @test5a(%struct.S* byval align 16) nounwind ssp


; rdar://8713376 - This memcpy can't be eliminated.
define i32 @test5(i32 %x) nounwind ssp {
entry:
  %y = alloca %struct.S, align 16
  %tmp = bitcast %struct.S* %y to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %tmp, i8* bitcast (%struct.S* @sS to i8*), i64 32, i32 16, i1 false)
  %a = getelementptr %struct.S* %y, i64 0, i32 1, i64 0
  store i8 4, i8* %a
  call void @test5a(%struct.S* byval align 16 %y)
  ret i32 0
  ; CHECK: @test5(
  ; CHECK: store i8 4
  ; CHECK: call void @test5a(%struct.S* byval align 16 %y)
}

;; Noop memcpy should be zapped.
define void @test6(i8 *%P) {
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %P, i8* %P, i64 8, i32 4, i1 false)
  ret void
; CHECK: @test6
; CHECK-NEXT: ret void
}


; PR9794 - Should forward memcpy into byval argument even though the memcpy
; isn't itself 8 byte aligned.
%struct.p = type { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }

define i32 @test7(%struct.p* nocapture byval align 8 %q) nounwind ssp {
entry:
  %agg.tmp = alloca %struct.p, align 4
  %tmp = bitcast %struct.p* %agg.tmp to i8*
  %tmp1 = bitcast %struct.p* %q to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %tmp, i8* %tmp1, i64 48, i32 4, i1 false)
  %call = call i32 @g(%struct.p* byval align 8 %agg.tmp) nounwind
  ret i32 %call
; CHECK: @test7
; CHECK: call i32 @g(%struct.p* byval align 8 %q) nounwind
}

declare i32 @g(%struct.p* byval align 8)

declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture, i32, i32, i1) nounwind

