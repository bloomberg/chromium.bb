; RUN: llc < %s -march=arm -mattr=+v6t2 | FileCheck %s

declare i32 @llvm.cttz.i32(i32)

define i32 @f1(i32 %a) {
; CHECK: f1:
; CHECK: rbit
; CHECK: clz
  %tmp = call i32 @llvm.cttz.i32( i32 %a )
  ret i32 %tmp
}
