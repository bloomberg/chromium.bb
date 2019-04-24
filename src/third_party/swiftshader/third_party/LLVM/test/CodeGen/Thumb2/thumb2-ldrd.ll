; RUN: llc < %s -mtriple=thumbv7-apple-darwin -mattr=+thumb2 -regalloc=linearscan | FileCheck %s

@b = external global i64*

define i64 @t(i64 %a) nounwind readonly {
entry:
;CHECK: ldrd r2, r3, [r2]
	%0 = load i64** @b, align 4
	%1 = load i64* %0, align 4
	%2 = mul i64 %1, %a
	ret i64 %2
}
