; RUN: llc < %s -march=x86 | FileCheck %s

@ok = internal constant [4 x i8] c"%d\0A\00"
@no = internal constant [4 x i8] c"no\0A\00"

define i1 @test1(i32 %v1, i32 %v2) nounwind {
entry:
  %t = call {i32, i1} @llvm.smul.with.overflow.i32(i32 %v1, i32 %v2)
  %sum = extractvalue {i32, i1} %t, 0
  %obit = extractvalue {i32, i1} %t, 1
  br i1 %obit, label %overflow, label %normal

normal:
  %t1 = tail call i32 (i8*, ...)* @printf( i8* getelementptr ([4 x i8]* @ok, i32 0, i32 0), i32 %sum ) nounwind
  ret i1 true

overflow:
  %t2 = tail call i32 (i8*, ...)* @printf( i8* getelementptr ([4 x i8]* @no, i32 0, i32 0) ) nounwind
  ret i1 false
; CHECK: test1:
; CHECK: imull
; CHECK-NEXT: jo
}

define i1 @test2(i32 %v1, i32 %v2) nounwind {
entry:
  %t = call {i32, i1} @llvm.smul.with.overflow.i32(i32 %v1, i32 %v2)
  %sum = extractvalue {i32, i1} %t, 0
  %obit = extractvalue {i32, i1} %t, 1
  br i1 %obit, label %overflow, label %normal

overflow:
  %t2 = tail call i32 (i8*, ...)* @printf( i8* getelementptr ([4 x i8]* @no, i32 0, i32 0) ) nounwind
  ret i1 false

normal:
  %t1 = tail call i32 (i8*, ...)* @printf( i8* getelementptr ([4 x i8]* @ok, i32 0, i32 0), i32 %sum ) nounwind
  ret i1 true
; CHECK: test2:
; CHECK: imull
; CHECK-NEXT: jno
}

declare i32 @printf(i8*, ...) nounwind
declare {i32, i1} @llvm.smul.with.overflow.i32(i32, i32)

define i32 @test3(i32 %a, i32 %b) nounwind readnone {
entry:
	%tmp0 = add i32 %b, %a
	%tmp1 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %tmp0, i32 2)
	%tmp2 = extractvalue { i32, i1 } %tmp1, 0
	ret i32 %tmp2
; CHECK: test3:
; CHECK: addl
; CHECK-NEXT: addl
; CHECK-NEXT: ret
}

define i32 @test4(i32 %a, i32 %b) nounwind readnone {
entry:
	%tmp0 = add i32 %b, %a
	%tmp1 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %tmp0, i32 4)
	%tmp2 = extractvalue { i32, i1 } %tmp1, 0
	ret i32 %tmp2
; CHECK: test4:
; CHECK: addl
; CHECK: mull
; CHECK-NEXT: ret
}
