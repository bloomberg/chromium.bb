; RUN: llc < %s -march=x86 -mattr=sse41 -stack-alignment=16 -join-physregs > %t
; RUN: grep pmul %t | count 12
; RUN: grep mov %t | count 11

; The f() arguments in %xmm0 and %xmm1 cause an extra movdqa without -join-physregs.

define <4 x i32> @a(<4 x i32> %i) nounwind  {
        %A = mul <4 x i32> %i, < i32 117, i32 117, i32 117, i32 117 >
        ret <4 x i32> %A
}
define <2 x i64> @b(<2 x i64> %i) nounwind  {
        %A = mul <2 x i64> %i, < i64 117, i64 117 >
        ret <2 x i64> %A
}
define <4 x i32> @c(<4 x i32> %i, <4 x i32> %j) nounwind  {
        %A = mul <4 x i32> %i, %j
        ret <4 x i32> %A
}
define <2 x i64> @d(<2 x i64> %i, <2 x i64> %j) nounwind  {
        %A = mul <2 x i64> %i, %j
        ret <2 x i64> %A
}
; Use a call to force spills.
declare void @foo()
define <4 x i32> @e(<4 x i32> %i, <4 x i32> %j) nounwind  {
        call void @foo()
        %A = mul <4 x i32> %i, %j
        ret <4 x i32> %A
}
define <2 x i64> @f(<2 x i64> %i, <2 x i64> %j) nounwind  {
        call void @foo()
        %A = mul <2 x i64> %i, %j
        ret <2 x i64> %A
}
