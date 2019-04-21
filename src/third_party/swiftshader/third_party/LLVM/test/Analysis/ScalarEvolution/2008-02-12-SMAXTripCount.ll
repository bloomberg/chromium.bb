; RUN: opt < %s -scalar-evolution -analyze | grep {Loop %loop: backedge-taken count is (100 + (-100 smax %n))}
; PR2002

define void @foo(i8 %n) {
entry:
	br label %loop
loop:
	%i = phi i8 [ -100, %entry ], [ %i.inc, %next ]
	%cond = icmp slt i8 %i, %n
	br i1 %cond, label %next, label %return
next:
        %i.inc = add i8 %i, 1
	br label %loop
return:
	ret void
}
