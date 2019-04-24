; RUN: llc < %s -mtriple=x86_64-apple-darwin10 -show-mc-encoding | FileCheck %s

define i32 @test1(i32 %X, i32* %y) nounwind {
	%tmp = load i32* %y		; <i32> [#uses=1]
	%tmp.upgrd.1 = icmp eq i32 %tmp, 0		; <i1> [#uses=1]
	br i1 %tmp.upgrd.1, label %ReturnBlock, label %cond_true

cond_true:		; preds = %0
	ret i32 1

ReturnBlock:		; preds = %0
	ret i32 0
; CHECK: test1:
; CHECK: cmpl	$0, (%rsi)
}

define i32 @test2(i32 %X, i32* %y) nounwind {
	%tmp = load i32* %y		; <i32> [#uses=1]
	%tmp1 = shl i32 %tmp, 3		; <i32> [#uses=1]
	%tmp1.upgrd.2 = icmp eq i32 %tmp1, 0		; <i1> [#uses=1]
	br i1 %tmp1.upgrd.2, label %ReturnBlock, label %cond_true

cond_true:		; preds = %0
	ret i32 1

ReturnBlock:		; preds = %0
	ret i32 0
; CHECK: test2:
; CHECK: movl	(%rsi), %eax
; CHECK: shll	$3, %eax
; CHECK: testl	%eax, %eax
}

define i64 @test3(i64 %x) nounwind {
  %t = icmp eq i64 %x, 0
  %r = zext i1 %t to i64
  ret i64 %r
; CHECK: test3:
; CHECK: 	testq	%rdi, %rdi
; CHECK: 	sete	%al
; CHECK: 	movzbl	%al, %eax
; CHECK: 	ret
}

define i64 @test4(i64 %x) nounwind {
  %t = icmp slt i64 %x, 1
  %r = zext i1 %t to i64
  ret i64 %r
; CHECK: test4:
; CHECK: 	testq	%rdi, %rdi
; CHECK: 	setle	%al
; CHECK: 	movzbl	%al, %eax
; CHECK: 	ret
}


define i32 @test5(double %A) nounwind  {
 entry:
 %tmp2 = fcmp ogt double %A, 1.500000e+02; <i1> [#uses=1]
 %tmp5 = fcmp ult double %A, 7.500000e+01; <i1> [#uses=1]
 %bothcond = or i1 %tmp2, %tmp5; <i1> [#uses=1]
 br i1 %bothcond, label %bb8, label %bb12

 bb8:; preds = %entry
 %tmp9 = tail call i32 (...)* @foo( ) nounwind ; <i32> [#uses=1]
 ret i32 %tmp9

 bb12:; preds = %entry
 ret i32 32
; CHECK: test5:
; CHECK: ucomisd	LCPI4_0(%rip), %xmm0
; CHECK: ucomisd	LCPI4_1(%rip), %xmm0
}

declare i32 @foo(...)

define i32 @test6() nounwind align 2 {
  %A = alloca {i64, i64}, align 8
  %B = getelementptr inbounds {i64, i64}* %A, i64 0, i32 1
  %C = load i64* %B
  %D = icmp eq i64 %C, 0
  br i1 %D, label %T, label %F
T:
  ret i32 1
  
F:
  ret i32 0
; CHECK: test6:
; CHECK: cmpq	$0, -8(%rsp)
; CHECK: encoding: [0x48,0x83,0x7c,0x24,0xf8,0x00]
}

