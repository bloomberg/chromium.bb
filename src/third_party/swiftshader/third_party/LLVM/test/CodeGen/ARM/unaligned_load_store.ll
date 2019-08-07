; RUN: llc < %s -march=arm -pre-RA-sched=source | FileCheck %s -check-prefix=GENERIC
; RUN: llc < %s -mtriple=armv6-apple-darwin | FileCheck %s -check-prefix=DARWIN_V6
; RUN: llc < %s -mtriple=armv6-apple-darwin -arm-strict-align | FileCheck %s -check-prefix=GENERIC
; RUN: llc < %s -mtriple=armv6-linux | FileCheck %s -check-prefix=GENERIC

; rdar://7113725

define void @t(i8* nocapture %a, i8* nocapture %b) nounwind {
entry:
; GENERIC: t:
; GENERIC: ldrb [[R2:r[0-9]+]]
; GENERIC: ldrb [[R3:r[0-9]+]]
; GENERIC: ldrb [[R12:r[0-9]+]]
; GENERIC: ldrb [[R1:r[0-9]+]]
; GENERIC: strb [[R1]]
; GENERIC: strb [[R12]]
; GENERIC: strb [[R3]]
; GENERIC: strb [[R2]]

; DARWIN_V6: t:
; DARWIN_V6: ldr r1
; DARWIN_V6: str r1

  %__src1.i = bitcast i8* %b to i32*              ; <i32*> [#uses=1]
  %__dest2.i = bitcast i8* %a to i32*             ; <i32*> [#uses=1]
  %tmp.i = load i32* %__src1.i, align 1           ; <i32> [#uses=1]
  store i32 %tmp.i, i32* %__dest2.i, align 1
  ret void
}
