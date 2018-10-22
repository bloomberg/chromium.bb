; RUN: llc < %s -march=x86-64 -mattr=+mmx,+sse2 | grep pxor
; RUN: llc < %s -march=x86-64 -mattr=+mmx,+sse2 | grep punpckldq

	%struct.vS1024 = type { [8 x <4 x i32>] }
	%struct.vS512 = type { [4 x <4 x i32>] }

declare x86_mmx @llvm.x86.mmx.psrli.q(x86_mmx, i32) nounwind readnone

define void @t() nounwind {
entry:
	br label %bb554

bb554:		; preds = %bb554, %entry
	%sum.0.reg2mem.0 = phi <1 x i64> [ %tmp562, %bb554 ], [ zeroinitializer, %entry ]		; <<1 x i64>> [#uses=1]
	%0 = load x86_mmx* null, align 8		; <<1 x i64>> [#uses=2]
	%1 = bitcast x86_mmx %0 to <2 x i32>		; <<2 x i32>> [#uses=1]
	%tmp555 = and <2 x i32> %1, < i32 -1, i32 0 >		; <<2 x i32>> [#uses=1]
	%2 = bitcast <2 x i32> %tmp555 to x86_mmx		; <<1 x i64>> [#uses=1]
	%3 = call x86_mmx @llvm.x86.mmx.psrli.q(x86_mmx %0, i32 32) nounwind readnone		; <<1 x i64>> [#uses=1]
        store <1 x i64> %sum.0.reg2mem.0, <1 x i64>* null
        %tmp3 = bitcast x86_mmx %2 to <1 x i64>
	%tmp558 = add <1 x i64> %sum.0.reg2mem.0, %tmp3		; <<1 x i64>> [#uses=1]
        %tmp5 = bitcast <1 x i64> %tmp558 to x86_mmx
	%4 = call x86_mmx @llvm.x86.mmx.psrli.q(x86_mmx %tmp5, i32 32) nounwind readnone		; <<1 x i64>> [#uses=1]
        %tmp6 = bitcast x86_mmx %4 to <1 x i64>
        %tmp7 = bitcast x86_mmx %3 to <1 x i64>
	%tmp562 = add <1 x i64> %tmp6, %tmp7		; <<1 x i64>> [#uses=1]
	br label %bb554
}
