; RUN: llc < %s -march=ppc32 | FileCheck %s -check-prefix=X32
; RUN: llc < %s -march=ppc64 | FileCheck %s -check-prefix=X64


define void @STWBRX(i32 %i, i8* %ptr, i32 %off) {
        %tmp1 = getelementptr i8* %ptr, i32 %off                ; <i8*> [#uses=1]
        %tmp1.upgrd.1 = bitcast i8* %tmp1 to i32*               ; <i32*> [#uses=1]
        %tmp13 = tail call i32 @llvm.bswap.i32( i32 %i )                ; <i32> [#uses=1]
        store i32 %tmp13, i32* %tmp1.upgrd.1
        ret void
}

define i32 @LWBRX(i8* %ptr, i32 %off) {
        %tmp1 = getelementptr i8* %ptr, i32 %off                ; <i8*> [#uses=1]
        %tmp1.upgrd.2 = bitcast i8* %tmp1 to i32*               ; <i32*> [#uses=1]
        %tmp = load i32* %tmp1.upgrd.2          ; <i32> [#uses=1]
        %tmp14 = tail call i32 @llvm.bswap.i32( i32 %tmp )              ; <i32> [#uses=1]
        ret i32 %tmp14
}

define void @STHBRX(i16 %s, i8* %ptr, i32 %off) {
        %tmp1 = getelementptr i8* %ptr, i32 %off                ; <i8*> [#uses=1]
        %tmp1.upgrd.3 = bitcast i8* %tmp1 to i16*               ; <i16*> [#uses=1]
        %tmp5 = call i16 @llvm.bswap.i16( i16 %s )              ; <i16> [#uses=1]
        store i16 %tmp5, i16* %tmp1.upgrd.3
        ret void
}

define i16 @LHBRX(i8* %ptr, i32 %off) {
        %tmp1 = getelementptr i8* %ptr, i32 %off                ; <i8*> [#uses=1]
        %tmp1.upgrd.4 = bitcast i8* %tmp1 to i16*               ; <i16*> [#uses=1]
        %tmp = load i16* %tmp1.upgrd.4          ; <i16> [#uses=1]
        %tmp6 = call i16 @llvm.bswap.i16( i16 %tmp )            ; <i16> [#uses=1]
        ret i16 %tmp6
}

declare i32 @llvm.bswap.i32(i32)

declare i16 @llvm.bswap.i16(i16)


; X32: stwbrx
; X32: lwbrx
; X32: sthbrx
; X32: lhbrx

; X64: stwbrx
; X64: lwbrx
; X64: sthbrx
; X64: lhbrx

