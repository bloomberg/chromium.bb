/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

        .global template_func
        .global template_func_end
template_func:
#if defined(__i386__)
        movl $1234, %eax
        popl %ecx
        nacljmp %ecx
#elif defined(__x86_64__)
        movq $1234, %rax
        popq %r11
        nacljmp %r11d, %r15
#else
# error "Unsupported architecture"
#endif
template_func_end:


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
        .fill 31 - 5, 1, 0x90
        hlt
branch_forwards_end:
branch_backwards:
        jmp branch_forwards
        /* The assembler generates a bad jmp if I use ".p2align 5"
           instead of padding manually. */
        .fill 31 - 5, 1, 0x90
        hlt
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
