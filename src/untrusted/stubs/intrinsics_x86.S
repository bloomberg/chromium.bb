
/* x86-32 implementations of GCC Atomic Builtins. */
/* Assemble with /usr/bin/as --32 -march=i686 intrinsics_x86.S  */

        .text

        .p2align NACLENTRYALIGN
        .global __sync_val_compare_and_swap_4

#if !defined(__x86_64__)

/* int __sync_val_compare_and_swap(int *ptr, int before, int after) */
__sync_val_compare_and_swap_4:
        movl     4(%esp), %edx   /* ptr */
        movl     8(%esp), %eax   /* before */
        movl     12(%esp), %ecx  /* after */
        lock
        cmpxchgl %ecx, (%edx)
        popl %ecx
        nacljmp %ecx


        .p2align NACLENTRYALIGN
        .global __sync_fetch_and_add_4

/* int __sync_fetch_and_add(int *ptr, int incr) */
__sync_fetch_and_add_4:
        movl    4(%esp), %edx  /* ptr */
        movl    8(%esp), %eax  /* incr */
        lock
        xaddl   %eax, (%edx)
        popl %ecx
        nacljmp %ecx

#else

/* int __sync_val_compare_and_swap(int *ptr, int before, int after) */
__sync_val_compare_and_swap_4:
        /* ptr is in %edi,  before in %esi,  after in %edx */
        movl     %esi, %eax   /* cmpxchgl expects operand in eax */
        movl     %edi, %edi   /* clear upper 32-bits of rdi */
        lock
        cmpxchgl %edx, (%r15,%rdi,1)
        popq %rcx
        nacljmp %ecx,%r15


        .p2align NACLENTRYALIGN
        .global __sync_fetch_and_add_4

/* int __sync_fetch_and_add(int *ptr, int incr) */
__sync_fetch_and_add_4:
        /* ptr is in %edi,  incr is in %esi */
        movl %edi, %edi   /* clear upper 32-bits of rdi */
        lock
        xaddl   %esi, (%r15,%rdi,1)
        popq %rcx
        nacljmp %ecx,%r15

#endif
