; RUN: opt < %s -instcombine -S | grep {store volatile}

define void @test() {
	%votf = alloca <4 x float>		; <<4 x float>*> [#uses=1]
	volatile store <4 x float> zeroinitializer, <4 x float>* %votf, align 16
	ret void
}

