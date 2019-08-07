; RUN: llc < %s -march=x86 | \
; RUN:   grep {s\[ah\]\[rl\]l} | count 1

define i32* @test1(i32* %P, i32 %X) nounwind {
        %Y = lshr i32 %X, 2             ; <i32> [#uses=1]
        %gep.upgrd.1 = zext i32 %Y to i64               ; <i64> [#uses=1]
        %P2 = getelementptr i32* %P, i64 %gep.upgrd.1           ; <i32*> [#uses=1]
        ret i32* %P2
}

define i32* @test2(i32* %P, i32 %X) nounwind {
        %Y = shl i32 %X, 2              ; <i32> [#uses=1]
        %gep.upgrd.2 = zext i32 %Y to i64               ; <i64> [#uses=1]
        %P2 = getelementptr i32* %P, i64 %gep.upgrd.2           ; <i32*> [#uses=1]
        ret i32* %P2
}

define i32* @test3(i32* %P, i32 %X) nounwind {
        %Y = ashr i32 %X, 2             ; <i32> [#uses=1]
        %P2 = getelementptr i32* %P, i32 %Y             ; <i32*> [#uses=1]
        ret i32* %P2
}

define fastcc i32 @test4(i32* %d) nounwind {
  %tmp4 = load i32* %d
  %tmp512 = lshr i32 %tmp4, 24
  ret i32 %tmp512
}
