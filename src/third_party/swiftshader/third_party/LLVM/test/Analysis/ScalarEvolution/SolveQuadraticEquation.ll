; RUN: opt < %s -analyze -scalar-evolution \
; RUN:   -scalar-evolution-max-iterations=0 | FileCheck %s

; PR1101

@A = weak global [1000 x i32] zeroinitializer, align 32         

define void @test1(i32 %N) {
entry:
        %"alloca point" = bitcast i32 0 to i32           ; <i32> [#uses=0]
        br label %bb3

bb:             ; preds = %bb3
        %tmp = getelementptr [1000 x i32]* @A, i32 0, i32 %i.0          ; <i32*> [#uses=1]
        store i32 123, i32* %tmp
        %tmp2 = add i32 %i.0, 1         ; <i32> [#uses=1]
        br label %bb3

bb3:            ; preds = %bb, %entry
        %i.0 = phi i32 [ 2, %entry ], [ %tmp2, %bb ]            ; <i32> [#uses=3]
        %SQ = mul i32 %i.0, %i.0
        %tmp4 = mul i32 %i.0, 2
        %tmp5 = sub i32 %SQ, %tmp4
        %tmp3 = icmp sle i32 %tmp5, 9999          ; <i1> [#uses=1]
        br i1 %tmp3, label %bb, label %bb5

bb5:            ; preds = %bb3
        br label %return

return:         ; preds = %bb5
        ret void
}
; CHECK: Determining loop execution counts for: @test1
; CHECK-NEXT: backedge-taken count is 100


; PR10383
; These next two used to crash.

define void @test2(i1 %cmp, i64 %n) {
entry:
  br label %for.body1

for.body1:
  %a0.08 = phi i64 [ 0, %entry ], [ %inc512, %for.body1 ]
  %inc512 = add i64 %a0.08, 1
  br i1 %cmp, label %preheader, label %for.body1

preheader:
  br label %for.body2

for.body2:
  %indvar = phi i64 [ 0, %preheader ], [ %indvar.next, %for.body2 ]
  %tmp111 = add i64 %n, %indvar
  %tmp114 = mul i64 %a0.08, %indvar
  %mul542 = mul i64 %tmp114, %tmp111
  %indvar.next = add i64 %indvar, 1
  br i1 undef, label %end, label %for.body2

end:
  ret void
}
; CHECK: Determining loop execution counts for: @test2

define i32 @test3() {
if.then466:
  br i1 undef, label %for.cond539.preheader, label %for.inc479

for.inc479:
  %a2.07 = phi i32 [ %add495, %for.inc479 ], [ 0, %if.then466 ]
  %j.36 = phi i32 [ %inc497, %for.inc479 ], [ undef, %if.then466 ]
  %mul484 = mul nsw i32 %j.36, %j.36
  %mul491 = mul i32 %j.36, %j.36
  %mul493 = mul i32 %mul491, %mul484
  %add495 = add nsw i32 %mul493, %a2.07
  %inc497 = add nsw i32 %j.36, 1
  br i1 undef, label %for.cond539.preheader, label %for.inc479

for.cond539.preheader:
  unreachable
}
; CHECK: Determining loop execution counts for: @test3
