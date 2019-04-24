; RUN: llc < %s -march=x86-64 -stats  |& \
; RUN:   grep {5 .*Number of machine instrs printed}

;; Integer absolute value, should produce something at least as good as:
;;       movl %edi, %ecx
;;       sarl $31, %ecx
;;       leal (%rdi,%rcx), %eax
;;       xorl %ecx, %eax
;;       ret
define i32 @test(i32 %a) nounwind {
        %tmp1neg = sub i32 0, %a
        %b = icmp sgt i32 %a, -1
        %abs = select i1 %b, i32 %a, i32 %tmp1neg
        ret i32 %abs
}

