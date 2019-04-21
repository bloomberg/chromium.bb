; RUN: llc < %s -mtriple=armv6-linux-gnu -regalloc=linearscan | FileCheck %s
; RUN: llc < %s -mtriple=armv6-linux-gnu -regalloc=basic | FileCheck %s

; The greedy register allocator uses a single CSR here, invalidating the test.

@b = external global i64*

define i64 @t(i64 %a) nounwind readonly {
entry:
; CHECK: push {lr}
; CHECK: pop {lr}
	%0 = load i64** @b, align 4
	%1 = load i64* %0, align 4
	%2 = mul i64 %1, %a
	ret i64 %2
}
