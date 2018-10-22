; RUN: llc < %s -mtriple=thumbv7-apple-darwin10 -march=thumb -mcpu=cortex-m3 | FileCheck %s -check-prefix=CORTEXM3
; RUN: llc < %s -mtriple=thumbv7-apple-darwin10 -march=thumb -mcpu=cortex-m4 | FileCheck %s -check-prefix=CORTEXM4
; RUN: llc < %s -mtriple=thumbv7-apple-darwin10 -march=thumb -mcpu=cortex-a8 | FileCheck %s -check-prefix=CORTEXA8


define float @foo(float %a, float %b) {
entry:
; CHECK: foo
; CORTEXM3: blx ___mulsf3
; CORTEXM4: vmul.f32  s0, s1, s0
; CORTEXA8: vmul.f32  d0, d1, d0
  %0 = fmul float %a, %b
  ret float %0
}

define double @bar(double %a, double %b) {
entry:
; CHECK: bar
  %0 = fmul double %a, %b
; CORTEXM3: blx ___muldf3
; CORTEXM4: blx ___muldf3
; CORTEXA8: vmul.f64  d16, d17, d16
  ret double %0
}
