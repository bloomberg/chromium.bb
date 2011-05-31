/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

        .text
start_of_text:

        .p2align NACLENTRYALIGN
write_num_10:
        movl 8(%esp), %eax  /* number to be printed */
        pushl %ebp
        pushl %ebx          /* callee saved */
        pushl %edi          /* callee saved */
        movl %esp, %ebp
        subl $0x10, %esp    /* buffer[0x10] */
        movl %ebp, %ebx     /* %ebx = &buffer[0x10] (off the end) */
        xorl %ecx, %ecx     /* %ecx = 0;  (byte count) */

        movl $10, %edi
do_digit_10:
        xorl %edx, %edx
        divl %edi           /* %edx:%eax div_mod 10 */
        addl $'0', %edx     /* *--%ebx = remainder + '0' */
        subl $1, %ebx
        movb %dl, (%ebx)
        addl $1, %ecx       /* ++byte_count */
        andl %eax, %eax
        jnz do_digit_10

        pushl %ecx
        pushl %ebx
        movl 0x10(%ebp), %eax
        pushl %eax
        call NACL_SYSCALL_ADDR(NACL_sys_write)
        addl $12, %esp

        movl %ebp, %esp
        popl %edi
        popl %ebx
        popl %ebp

        popl %ecx
        nacljmp %ecx

write_num_16:
        movl 8(%esp), %eax  /* number to be printed */
        pushl %ebp
        pushl %ebx
        movl %esp, %ebp
        subl $0x10, %esp
        movl %ebp, %ebx
        xorl %ecx, %ecx

do_digit_16:
        movl %eax, %edx
        andl $0xf, %edx
        addl $'0', %edx
        cmpl $(10 + '0'), %edx
        jl do_digit_not_upper
        addl $('a'-'0'-10), %edx
do_digit_not_upper:
        subl $1, %ebx
        movb %dl, (%ebx)
        addl $1, %ecx
        shrl $4, %eax
        jnz do_digit_16

        pushl %ecx
        pushl %ebx
        movl 0xc(%ebp), %eax
        pushl %eax
        call NACL_SYSCALL_ADDR(NACL_sys_write)
        addl $12, %esp

        movl %ebp, %esp
        popl %ebx
        popl %ebp

        popl %ecx
        nacljmp %ecx

        .p2align NACLENTRYALIGN
write_char:
        movl 4(%esp), %eax
        movl 8(%esp), %edx
        push %ebp
        movl %esp, %ebp
        subl $4, %esp
        lea -4(%ebp), %ecx
        movb %dl, (%ecx)
        pushl $1
        pushl %ecx
        pushl %eax
        call NACL_SYSCALL_ADDR(NACL_sys_write)
        addl $12, %esp
        movl %ebp, %esp
        popl %ebp
        popl %ecx
        nacljmp %ecx

        .p2align NACLENTRYALIGN
        .globl _start
_start:
        pushl $1234567
        pushl $1
        call write_num_10
        addl $8, %esp

        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp

        pushl $0xdeadbeef
        pushl $1
        call write_num_16
        addl $8, %esp

        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp

        pushl $end_of_text
        pushl $1
        call write_num_16
        addl $8, %esp
        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp

        pushl $0
        call NACL_SYSCALL_ADDR(NACL_sys_sysbrk)
        addl $4, %esp

        xorl %edi, %edi  /* edi holds our eventual exit status */
        cmpl $EXPECTED_BREAK, %eax
        je brk_sane
        movl $1, %edi
brk_sane:

        pushl %eax
        pushl $1
        call write_num_16
        addl $8, %esp

        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp

        pushl $EXPECTED_BREAK
        pushl $1
        call write_num_16
        addl $8, %esp

        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp

#if EXPECTED_RODATA != 0
        pushl $12
        pushl $ro_str
        pushl $1
        call NACL_SYSCALL_ADDR(NACL_sys_write)
        addl $12, %esp

        pushl $ro_str
        pushl $1
        call write_num_16
        addl $8, %esp

        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp

        leal ro_str, %eax
        cmpl $EXPECTED_RODATA, %eax
        je ro_sane
        movl $1, %edi
ro_sane:

        pushl $EXPECTED_RODATA
        pushl $1
        call write_num_16
        addl $8, %esp

        pushl $'\n'
        pushl $1
        call write_char
        addl $8, %esp
#endif

        pushl %edi
        call NACL_SYSCALL_ADDR(NACL_sys_exit)

        .skip TEXT_EXTEND - (. - start_of_text), 0xf4
end_of_text:

#if EXPECTED_RODATA != 0
        .section .rodata
ro_str: .ascii "Hello world\n"
        /*      123456789012 */
#endif
