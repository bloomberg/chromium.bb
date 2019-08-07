; Test that the memcmpOptimizer works correctly
; RUN: opt < %s -simplify-libcalls -S | FileCheck %s

@h = constant [2 x i8] c"h\00"		; <[2 x i8]*> [#uses=0]
@hel = constant [4 x i8] c"hel\00"		; <[4 x i8]*> [#uses=0]
@hello_u = constant [8 x i8] c"hello_u\00"		; <[8 x i8]*> [#uses=0]

declare i32 @memcmp(i8*, i8*, i32)

define void @test(i8* %P, i8* %Q, i32 %N, i32* %IP, i1* %BP) {
	%A = call i32 @memcmp( i8* %P, i8* %P, i32 %N )		; <i32> [#uses=1]
; CHECK-NOT: call {{.*}} memcmp
; CHECK: store volatile
	store volatile i32 %A, i32* %IP
	%B = call i32 @memcmp( i8* %P, i8* %Q, i32 0 )		; <i32> [#uses=1]
; CHECK-NOT: call {{.*}} memcmp
; CHECK: store volatile
	store volatile i32 %B, i32* %IP
	%C = call i32 @memcmp( i8* %P, i8* %Q, i32 1 )		; <i32> [#uses=1]
; CHECK: load
; CHECK: zext
; CHECK: load
; CHECK: zext
; CHECK: sub
; CHECK: store volatile
	store volatile i32 %C, i32* %IP
  %F = call i32 @memcmp(i8* getelementptr ([4 x i8]* @hel, i32 0, i32 0),
                        i8* getelementptr ([8 x i8]* @hello_u, i32 0, i32 0),
                        i32 3)
; CHECK-NOT: call {{.*}} memcmp
; CHECK: store volatile
  store volatile i32 %F, i32* %IP
	ret void
}

