;test for SUBC and SUBE expansion
;
; RUN: llc < %s -march=alpha

define i128 @sub128(i128 %x, i128 %y) {
entry:
	%tmp = sub i128 %y, %x
	ret i128 %tmp
}
