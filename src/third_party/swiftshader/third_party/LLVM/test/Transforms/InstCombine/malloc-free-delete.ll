; RUN: opt < %s -instcombine -S | FileCheck %s
; PR1201
define i32 @main(i32 %argc, i8** %argv) {
    %c_19 = alloca i8*
    %malloc_206 = tail call i8* @malloc(i32 mul (i32 ptrtoint (i8* getelementptr (i8* null, i32 1) to i32), i32 10))
    store i8* %malloc_206, i8** %c_19
    %tmp_207 = load i8** %c_19
    tail call void @free(i8* %tmp_207)
    ret i32 0
; CHECK-NOT: malloc
; CHECK-NOT: free
; CHECK: ret i32 0
}

declare noalias i8* @malloc(i32)
declare void @free(i8*)

define i1 @foo() {
; CHECK: @foo
; CHECK-NEXT: ret i1 false
  %m = call i8* @malloc(i32 1)
  %z = icmp eq i8* %m, null
  call void @free(i8* %m)
  ret i1 %z
}

declare void @llvm.lifetime.start(i64, i8*)
declare void @llvm.lifetime.end(i64, i8*)

define void @test3() {
; CHECK: @test3
; CHECK-NEXT: ret void
  %a = call noalias i8* @malloc(i32 10)
  call void @llvm.lifetime.start(i64 10, i8* %a)
  call void @llvm.lifetime.end(i64 10, i8* %a)
  ret void
}

;; This used to crash.
define void @test4() {
; CHECK: @test4
; CHECK-NEXT: ret void
  %A = call i8* @malloc(i32 16000)
  %B = bitcast i8* %A to double*
  %C = bitcast double* %B to i8*
  call void @free(i8* %C)
  ret void
}
