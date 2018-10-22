; Make sure this testcase codegens to the ornot instruction
; RUN: llc < %s -march=alpha | grep ornot

define i64 @bar(i64 %x, i64 %y) {
entry:
        %tmp.1 = xor i64 %x, -1         ; <i64> [#uses=1]
        %tmp.2 = or i64 %y, %tmp.1              ; <i64> [#uses=1]
        ret i64 %tmp.2
}

