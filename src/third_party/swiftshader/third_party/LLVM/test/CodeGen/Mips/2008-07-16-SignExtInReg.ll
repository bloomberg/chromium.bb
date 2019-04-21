; DISABLED: llc < %s -march=mips -o %t
; DISABLED: grep seh %t | count 1
; DISABLED: grep seb %t | count 1
; RUN: false
; XFAIL: *

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:32-i16:16:32-i32:32:32-i64:32:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64"
target triple = "mipsallegrexel-unknown-psp-elf"

define signext i8 @A(i8 %e.0, i8 signext %sum)  nounwind {
entry:
	add i8 %sum, %e.0		; <i8>:0 [#uses=1]
	ret i8 %0
}

define signext i16 @B(i16 %e.0, i16 signext %sum) nounwind {
entry:
	add i16 %sum, %e.0		; <i16>:0 [#uses=1]
	ret i16 %0
}

