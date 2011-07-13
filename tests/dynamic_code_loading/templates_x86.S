/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

        .global template_func
        .global template_func_end
template_func:
#if defined(__i386__)
        movl $1234, %eax
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%ecx
        call template_func
#elif defined(__x86_64__)
        disp = template_func_end - 4
        xorl %r11d, %r11d
        movq disp(%r15,%r11,1), %rax
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call template_func
        test $0xffffffff,%r11d
        movq $1234, %rax
#else
# error "Unsupported architecture"
#endif
template_func_end:


        .global template_func_replacement
        .global template_func_replacement_end
        .p2align 5
template_func_replacement:
#if defined(__i386__)
        movl $4321, %eax                       /* replaces constant */
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%ecx
        call (template_func_replacement - 32)  /* replaces a call target, the
                                                * new target is bundle aligned
                                                * and target address outside
                                                * modified section */
#elif defined(__x86_64__)
        /* tests all modifications that are
         * currently allowed by service runtime */
        new_disp = template_func_replacement_end - 4
        xorl %r11d, %r11d
        movq new_disp(%r15,%r11,1), %rax       /* replaces displacement */
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call (template_func_replacement - 32)  /* replaces a call target, the
                                                * new target is bundle aligned
                                                * and target address outside
                                                * modified section  */
        test $0xffffffff,%r11d
        movq $4321, %rax                       /* replaces constant */
#else
# error "Unsupported architecture"
#endif
template_func_replacement_end:


        .global template_func_nonreplacement
        .global template_func_nonreplacement_end
        .global template_func_misaligned_replacement
        .global template_func_misaligned_replacement_end
        .p2align 5
template_func_nonreplacement:
template_func_misaligned_replacement:
#if defined(__i386__)
        nop                                    /* nop creates misalignment in
                                                * replacing section which makes
                                                * it illegal */
        movl $1234, %eax
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%ecx
        call template_func_misaligned_replacement
#elif defined(__x86_64__)
        nop                                    /* nop creates misalignment in
                                                * replacing section which makes
                                                * it illegal */
        xorl %r11d, %r11d
        movq disp(%r15,%r11,1), %rax
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call template_func_misaligned_replacement
        test $0xffffffff,%r11d
        movq $1234, %rax
#else
# error "Unsupported architecture"
#endif
template_func_misaligned_replacement_end:
template_func_nonreplacement_end:


        .global hlts
        .global hlts_end
        .p2align 5
hlts:
        .fill 32, 1, 0xf4
hlts_end:


        .global branch_forwards
        .global branch_forwards_end
        .global branch_backwards
        .global branch_backwards_end
        .p2align 5
branch_forwards:
        jmp branch_backwards
        /* The assembler generates a bad jmp if I use ".p2align 5"
           instead of padding manually.  TODO(mseaborn): Investigate. */
        .fill 32 - 5, 1, 0x90
branch_forwards_end:
branch_backwards:
        jmp branch_forwards
        /* The assembler generates a bad jmp if I use ".p2align 5"
           instead of padding manually. */
        .fill 32 - 5, 1, 0x90
branch_backwards_end:


        /* We include disallowed code below, so this must go into the
           data segment. */
        .data


        .global invalid_code
        .global invalid_code_end
        .p2align 5
invalid_code:
        int $0x80
        ret
invalid_code_end:


        .global template_func_illegal_register_replacement
        .global template_func_illegal_register_replacement_end
        .p2align 5
template_func_illegal_register_replacement:
#if defined(__i386__)
        movl $1234, %eax
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%edx                             /* replaces register here, jmp
                                                * becomes illegal */
        call template_func_illegal_register_replacement
#elif defined(__x86_64__)
        xorl %r11d, %r11d
        movq disp(%r15,%r11,1), %rax
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call template_func_illegal_register_replacement
        test $0xffffffff,%r11d
        movq $1234, %rbx                       /* replaces register
                                                * which is not allowed */
#else
# error "Unsupported architecture"
#endif
template_func_illegal_register_replacement_end:


        .global template_func_illegal_guard_replacement
        .global template_func_illegal_guard_replacement_end
        .p2align 5
template_func_illegal_guard_replacement:
#if defined(__i386__)
        movl $1234, %eax
        popl %ecx
        and  $0xffffffff,%ecx                  /* modifies mask */
        jmp  *%ecx
        call template_func_illegal_guard_replacement
#elif defined(__x86_64__)
        xorl %r14d, %r14d                      /* modifies memory guard */
        movq disp(%r15,%r11,1), %rax
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call template_func_illegal_guard_replacement
        test $0xffffffff,%r11d
        movq $1234, %rax
#else
# error "Unsupported architecture"
#endif
template_func_illegal_guard_replacement_end:


        .global template_func_illegal_call_target
        .global template_func_illegal_call_target_end
        .p2align 5
template_func_illegal_call_target:
#if defined(__i386__)
        movl $1234, %eax
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%ecx
        call (template_func_illegal_call_target - 31)  /* target of a call
                                                * instruction is beyond
                                                * replaced section, and it is
                                                * not bundle_aligned */
#elif defined(__x86_64__)
        xorl %r11d, %r11d
        movq disp(%r15,%r11,1), %rax
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call (template_func_illegal_call_target - 31)  /* target of a call
                                                * instruction is beyond
                                                * replaced section, and it is
                                                * not bundle_aligned */
        test $0xffffffff,%r11d
        movq $1234, %rax
#else
# error "Unsupported architecture"
#endif
template_func_illegal_call_target_end:


        .global template_func_illegal_constant_replacement
        .global template_func_illegal_constant_replacement_end
        .p2align 5
template_func_illegal_constant_replacement:
#if defined(__i386__)
        hlt  /* not applicable */
#elif defined(__x86_64__)
        xorl %r11d, %r11d
        movq disp(%r15,%r11,1), %rax
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
        call (template_func_illegal_constant_replacement)
        test $0xf0f0f0f0,%r11d             /* can't change constant in test */
        movq $1234, %rax
#else
# error "Unsupported architecture"
#endif
template_func_illegal_constant_replacement_end:

        .global template_func_external_jump_target
        .global template_func_external_jump_target_end
        .p2align 5
template_func_external_jump_target:
#if defined(__i386__)
        movl $1234, %eax
        jmp external_jump_return
        call template_func_external_jump_target
        movl $9876, %eax
external_jump_return:
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%ecx
#elif defined(__x86_64__)
        movq $1234, %rax
        jmp external_jump_return
        call template_func_external_jump_target
        movq $9876, %rax
external_jump_return:
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
#else
# error "Unsupported architecture"
#endif
template_func_external_jump_target_end:

        .global template_func_external_jump_target_replace
        .global template_func_external_jump_target_replace_end
        .p2align 5
template_func_external_jump_target_replace:
#if defined(__i386__)
        movl $4321, %eax
        jmp external_jump_replace_return
        call template_func_external_jump_target_replace
        movl $9876, %eax
external_jump_replace_return:
        popl %ecx
        and  $0xffffffe0,%ecx
        jmp  *%ecx
#elif defined(__x86_64__)
        movq $4321, %rax
        jmp external_jump_replace_return
        call template_func_external_jump_target_replace
        movq $9876, %rax
external_jump_replace_return:
        popq %r11
        andl $0xffffffe0,%r11d
        addq %r15,%r11
        jmpq *%r11
#else
# error "Unsupported architecture"
#endif
template_func_external_jump_target_replace_end:
.global template_instr
.global template_instr_end
.global template_instr_replace
.global template_instr_replace_end
template_instr:
        mov $0,%eax
template_instr_end:
template_instr_replace:
        mov $0x11111111,%eax
template_instr_replace_end:

