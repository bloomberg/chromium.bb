; Test to make sure that the 'private' is used correctly.
;
; RUN: llc < %s -march=xcore > %t
; RUN: grep .Lfoo: %t
; RUN: grep bl.*\.Lfoo %t
; RUN: grep .Lbaz: %t
; RUN: grep ldw.*\.Lbaz %t

define private void @foo() {
        ret void
}

@baz = private global i32 4

define i32 @bar() {
        call void @foo()
	%1 = load i32* @baz, align 4
        ret i32 %1
}
