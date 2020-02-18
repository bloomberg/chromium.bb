; Show that we can take advantage of the vmla instruction for floating point
; operations during optimization.

; Note that we use -O2 to force the result of the fmul to be (immediately)
; available for the fadd. When using -Om1, the merge of fmul and fadd does not
; happen due to intervening register spill code.

; REQUIRES: allow_dump

; Compile using standalone assembler.
; RUN: %p2i --filetype=asm -i %s --target=arm32 --args -O2 \
; RUN:   -reg-use=s20,s21,s22,d20,d21,d22 \
; RUN:   | FileCheck %s --check-prefix=ASM

; Show bytes in assembled standalone code.
; RUN: %p2i --filetype=asm -i %s --target=arm32 --assemble --disassemble \
; RUN:   --args -O2 -reg-use=s20,s21,s22,d20,d21,d22 \
; RUN:   | FileCheck %s --check-prefix=DIS

; Compile using integrated assembler.
; RUN: %p2i --filetype=iasm -i %s --target=arm32 --args -O2 \
; RUN:   -reg-use=s20,s21,s22,d20,d21,d22 \
; RUN:   | FileCheck %s --check-prefix=IASM

; Show bytes in assembled integrated code.
; RUN: %p2i --filetype=iasm -i %s --target=arm32 --assemble --disassemble \
; RUN:   --args -O2 -reg-use=s20,s21,s22,d20,d21,d22 \
; RUN:   | FileCheck %s --check-prefix=DIS

define internal float @mulAddFloat(float %f1, float %f2) {
; ASM-LABEL: mulAddFloat:
; DIS-LABEL: 00000000 <mulAddFloat>:

  %v1 = fmul float %f1, 1.5
  %v2 = fadd float %f2, %v1

; ASM:  vmla.f32        s21, s20, s22
; DIS:   10:    ee4aaa0b
; IASM-NOT: vmla

  ret float %v2
}

define internal double @mulAddDouble(double %f1, double %f2) {
; ASM-LABEL: mulAddDouble:
; DIS-LABEL: 00000020 <mulAddDouble>:

  %v1 = fmul double %f1, 1.5
  %v2 = fadd double %f2, %v1

; ASM:  vmla.f64        d21, d20, d22
; DIS:   2c:    ee445ba6
; IASM-NOT: vmla

  ret double %v2
}
