; RUN: opt < %s -functionattrs -S | not grep {nocapture *%%q}
; RUN: opt < %s -functionattrs -S | grep {nocapture *%%p} | count 6
@g = global i32* null		; <i32**> [#uses=1]

define i32* @c1(i32* %q) {
	ret i32* %q
}

define void @c2(i32* %q) {
	store i32* %q, i32** @g
	ret void
}

define void @c3(i32* %q) {
	call void @c2(i32* %q)
	ret void
}

define i1 @c4(i32* %q, i32 %bitno) {
	%tmp = ptrtoint i32* %q to i32
	%tmp2 = lshr i32 %tmp, %bitno
	%bit = trunc i32 %tmp2 to i1
	br i1 %bit, label %l1, label %l0
l0:
	ret i1 0 ; escaping value not caught by def-use chaining.
l1:
	ret i1 1 ; escaping value not caught by def-use chaining.
}

@lookup_table = global [2 x i1] [ i1 0, i1 1 ]

define i1 @c5(i32* %q, i32 %bitno) {
	%tmp = ptrtoint i32* %q to i32
	%tmp2 = lshr i32 %tmp, %bitno
	%bit = and i32 %tmp2, 1
        ; subtle escape mechanism follows
	%lookup = getelementptr [2 x i1]* @lookup_table, i32 0, i32 %bit
	%val = load i1* %lookup
	ret i1 %val
}

declare void @throw_if_bit_set(i8*, i8) readonly
define i1 @c6(i8* %q, i8 %bit) {
	invoke void @throw_if_bit_set(i8* %q, i8 %bit)
		to label %ret0 unwind label %ret1
ret0:
	ret i1 0
ret1:
        %exn = landingpad {i8*, i32} personality i32 (...)* @__gxx_personality_v0
                 cleanup
	ret i1 1
}

declare i32 @__gxx_personality_v0(...)

define i1* @lookup_bit(i32* %q, i32 %bitno) readnone nounwind {
	%tmp = ptrtoint i32* %q to i32
	%tmp2 = lshr i32 %tmp, %bitno
	%bit = and i32 %tmp2, 1
	%lookup = getelementptr [2 x i1]* @lookup_table, i32 0, i32 %bit
	ret i1* %lookup
}

define i1 @c7(i32* %q, i32 %bitno) {
	%ptr = call i1* @lookup_bit(i32* %q, i32 %bitno)
	%val = load i1* %ptr
	ret i1 %val
}


define i32 @nc1(i32* %q, i32* %p, i1 %b) {
e:
	br label %l
l:
	%x = phi i32* [ %p, %e ]
	%y = phi i32* [ %q, %e ]
	%tmp = bitcast i32* %x to i32*		; <i32*> [#uses=2]
	%tmp2 = select i1 %b, i32* %tmp, i32* %y
	%val = load i32* %tmp2		; <i32> [#uses=1]
	store i32 0, i32* %tmp
	store i32* %y, i32** @g
	ret i32 %val
}

define void @nc2(i32* %p, i32* %q) {
	%1 = call i32 @nc1(i32* %q, i32* %p, i1 0)		; <i32> [#uses=0]
	ret void
}

define void @nc3(void ()* %p) {
	call void %p()
	ret void
}

declare void @external(i8*) readonly nounwind
define void @nc4(i8* %p) {
	call void @external(i8* %p)
	ret void
}

define void @nc5(void (i8*)* %f, i8* %p) {
	call void %f(i8* %p) readonly nounwind
	call void %f(i8* nocapture %p)
	ret void
}
