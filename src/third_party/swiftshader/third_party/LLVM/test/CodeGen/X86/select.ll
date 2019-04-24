; RUN: llc < %s -mtriple=x86_64-apple-darwin10 | FileCheck %s
; PR5757

%0 = type { i64, i32 }

define i32 @test1(%0* %p, %0* %q, i1 %r) nounwind {
  %t0 = load %0* %p
  %t1 = load %0* %q
  %t4 = select i1 %r, %0 %t0, %0 %t1
  %t5 = extractvalue %0 %t4, 1
  ret i32 %t5
; CHECK: test1:
; CHECK: cmovneq %rdi, %rsi
; CHECK: movl (%rsi), %eax
}


; PR2139
define i32 @test2() nounwind {
entry:
	%tmp73 = tail call i1 @return_false()		; <i8> [#uses=1]
	%g.0 = select i1 %tmp73, i16 0, i16 -480		; <i16> [#uses=2]
	%tmp7778 = sext i16 %g.0 to i32		; <i32> [#uses=1]
	%tmp80 = shl i32 %tmp7778, 3		; <i32> [#uses=2]
	%tmp87 = icmp sgt i32 %tmp80, 32767		; <i1> [#uses=1]
	br i1 %tmp87, label %bb90, label %bb91
bb90:		; preds = %bb84, %bb72
	unreachable
bb91:		; preds = %bb84
	ret i32 0
; CHECK: test2:
; CHECK: movnew
; CHECK: movswl
}

declare i1 @return_false()


;; Select between two floating point constants.
define float @test3(i32 %x) nounwind readnone {
entry:
	%0 = icmp eq i32 %x, 0		; <i1> [#uses=1]
	%iftmp.0.0 = select i1 %0, float 4.200000e+01, float 2.300000e+01		; <float> [#uses=1]
	ret float %iftmp.0.0
; CHECK: test3:
; CHECK: movss	{{.*}},4), %xmm0
}

define signext i8 @test4(i8* nocapture %P, double %F) nounwind readonly {
entry:
	%0 = fcmp olt double %F, 4.200000e+01		; <i1> [#uses=1]
	%iftmp.0.0 = select i1 %0, i32 4, i32 0		; <i32> [#uses=1]
	%1 = getelementptr i8* %P, i32 %iftmp.0.0		; <i8*> [#uses=1]
	%2 = load i8* %1, align 1		; <i8> [#uses=1]
	ret i8 %2
; CHECK: test4:
; CHECK: movsbl	({{.*}},4), %eax
}

define void @test5(i1 %c, <2 x i16> %a, <2 x i16> %b, <2 x i16>* %p) nounwind {
  %x = select i1 %c, <2 x i16> %a, <2 x i16> %b
  store <2 x i16> %x, <2 x i16>* %p
  ret void
; CHECK: test5:
}

define void @test6(i32 %C, <4 x float>* %A, <4 x float>* %B) nounwind {
        %tmp = load <4 x float>* %A             ; <<4 x float>> [#uses=1]
        %tmp3 = load <4 x float>* %B            ; <<4 x float>> [#uses=2]
        %tmp9 = fmul <4 x float> %tmp3, %tmp3            ; <<4 x float>> [#uses=1]
        %tmp.upgrd.1 = icmp eq i32 %C, 0                ; <i1> [#uses=1]
        %iftmp.38.0 = select i1 %tmp.upgrd.1, <4 x float> %tmp9, <4 x float> %tmp               ; <<4 x float>> [#uses=1]
        store <4 x float> %iftmp.38.0, <4 x float>* %A
        ret void
; Verify that the fmul gets sunk into the one part of the diamond where it is
; needed.
; CHECK: test6:
; CHECK: jne
; CHECK: mulps
; CHECK: ret
; CHECK: ret
}

; Select with fp80's
define x86_fp80 @test7(i32 %tmp8) nounwind {
        %tmp9 = icmp sgt i32 %tmp8, -1          ; <i1> [#uses=1]
        %retval = select i1 %tmp9, x86_fp80 0xK4005B400000000000000, x86_fp80 0xK40078700000000000000
        ret x86_fp80 %retval
; CHECK: test7:
; CHECK: leaq
; CHECK: fldt (%r{{.}}x,%r{{.}}x)
}

; widening select v6i32 and then a sub
define void @test8(i1 %c, <6 x i32>* %dst.addr, <6 x i32> %src1,<6 x i32> %src2) nounwind {
	%x = select i1 %c, <6 x i32> %src1, <6 x i32> %src2
	%val = sub <6 x i32> %x, < i32 1, i32 1, i32 1, i32 1, i32 1, i32 1 >
	store <6 x i32> %val, <6 x i32>* %dst.addr
	ret void
        
; CHECK: test8:
}


;; Test integer select between values and constants.

define i64 @test9(i64 %x, i64 %y) nounwind readnone ssp noredzone {
  %cmp = icmp ne i64 %x, 0
  %cond = select i1 %cmp, i64 %y, i64 -1
  ret i64 %cond
; CHECK: test9:
; CHECK: cmpq	$1, %rdi
; CHECK: sbbq	%rax, %rax
; CHECK: orq	%rsi, %rax
; CHECK: ret
}

;; Same as test9
define i64 @test9a(i64 %x, i64 %y) nounwind readnone ssp noredzone {
  %cmp = icmp eq i64 %x, 0
  %cond = select i1 %cmp, i64 -1, i64 %y
  ret i64 %cond
; CHECK: test9a:
; CHECK: cmpq	$1, %rdi
; CHECK: sbbq	%rax, %rax
; CHECK: orq	%rsi, %rax
; CHECK: ret
}

define i64 @test9b(i64 %x, i64 %y) nounwind readnone ssp noredzone {
  %cmp = icmp eq i64 %x, 0
  %A = sext i1 %cmp to i64
  %cond = or i64 %y, %A
  ret i64 %cond
; CHECK: test9b:
; CHECK: cmpq	$1, %rdi
; CHECK: sbbq	%rax, %rax
; CHECK: orq	%rsi, %rax
; CHECK: ret
}

;; Select between -1 and 1.
define i64 @test10(i64 %x, i64 %y) nounwind readnone ssp noredzone {
  %cmp = icmp eq i64 %x, 0
  %cond = select i1 %cmp, i64 -1, i64 1
  ret i64 %cond
; CHECK: test10:
; CHECK: cmpq	$1, %rdi
; CHECK: sbbq	%rax, %rax
; CHECK: orq	$1, %rax
; CHECK: ret
}



define i64 @test11(i64 %x, i64 %y) nounwind readnone ssp noredzone {
  %cmp = icmp eq i64 %x, 0
  %cond = select i1 %cmp, i64 %y, i64 -1
  ret i64 %cond
; CHECK: test11:
; CHECK: cmpq	$1, %rdi
; CHECK: sbbq	%rax, %rax
; CHECK: notq %rax
; CHECK: orq	%rsi, %rax
; CHECK: ret
}

define i64 @test11a(i64 %x, i64 %y) nounwind readnone ssp noredzone {
  %cmp = icmp ne i64 %x, 0
  %cond = select i1 %cmp, i64 -1, i64 %y
  ret i64 %cond
; CHECK: test11a:
; CHECK: cmpq	$1, %rdi
; CHECK: sbbq	%rax, %rax
; CHECK: notq %rax
; CHECK: orq	%rsi, %rax
; CHECK: ret
}


declare noalias i8* @_Znam(i64) noredzone

define noalias i8* @test12(i64 %count) nounwind ssp noredzone {
entry:
  %A = tail call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %count, i64 4)
  %B = extractvalue { i64, i1 } %A, 1
  %C = extractvalue { i64, i1 } %A, 0
  %D = select i1 %B, i64 -1, i64 %C
  %call = tail call noalias i8* @_Znam(i64 %D) nounwind noredzone
  ret i8* %call
; CHECK: test12:
; CHECK: mulq
; CHECK: movq $-1, %rdi
; CHECK: cmovnoq	%rax, %rdi
; CHECK: jmp	__Znam
}

declare { i64, i1 } @llvm.umul.with.overflow.i64(i64, i64) nounwind readnone

define i32 @test13(i32 %a, i32 %b) nounwind {
  %c = icmp ult i32 %a, %b
  %d = sext i1 %c to i32
  ret i32 %d
; CHECK: test13:
; CHECK: cmpl
; CHECK-NEXT: sbbl
; CHECK-NEXT: ret
}

define i32 @test14(i32 %a, i32 %b) nounwind {
  %c = icmp uge i32 %a, %b
  %d = sext i1 %c to i32
  ret i32 %d
; CHECK: test14:
; CHECK: cmpl
; CHECK-NEXT: sbbl
; CHECK-NEXT: notl
; CHECK-NEXT: ret
}

