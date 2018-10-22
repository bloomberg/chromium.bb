; Test that even if a call parameter matches its declaration, it must still
; be a legal call parameter type (unless declaration is intrinsic).

; REQUIRES: no_minimal_build

; RUN: %p2i --expect-fail -i %s --insts --args \
; RUN:      -allow-externally-defined-symbols | FileCheck %s

declare void @f(i8);
; CHECK: Invalid type signature for f: void (i8)

define void @Test() {
entry:
  call void @f(i8 1)
  ret void
}
