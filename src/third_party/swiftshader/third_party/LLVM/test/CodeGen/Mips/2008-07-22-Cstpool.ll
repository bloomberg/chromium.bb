; RUN: llc < %s -march=mips -o %t
; RUN: grep {CPI\[01\]_\[01\]:} %t | count 2
; RUN: grep {.rodata.cst4,"aM",@progbits} %t | count 1
target datalayout = "e-p:32:32:32-i1:8:8-i8:8:32-i16:16:32-i32:32:32-i64:32:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64"
target triple = "mipsallegrexel-unknown-psp-elf"

define float @F(float %a) nounwind {
entry:
	fadd float %a, 0x4011333340000000		; <float>:0 [#uses=1]
	fadd float %0, 0x4010666660000000		; <float>:1 [#uses=1]
	ret float %1
}
