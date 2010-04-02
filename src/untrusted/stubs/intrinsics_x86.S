
/* x86-32 implementations of GCC Atomic Builtins. */
/* Assemble with /usr/bin/as --32 -march=i686 intrinsics_x86.S  */

        .text

/* TODO: use nacl macros for alignment */
        .align 32
        .global __sync_val_compare_and_swap_4

/* int __sync_val_compare_and_swap(int *ptr, int before, int after) */
__sync_val_compare_and_swap_4:
        movl     4(%esp), %edx   /* ptr */
        movl     8(%esp), %eax   /* before */
        movl     12(%esp), %ecx  /* after */
        lock
        cmpxchgl %ecx, (%edx)
        ret

/* TODO: use nacl macros for alignment */
        .align 32
        .global __sync_fetch_and_add_4

/* int __sync_fetch_and_add(int *ptr, int incr) */
__sync_fetch_and_add_4:
        movl    4(%esp), %edx  /* ptr */
        movl    8(%esp), %eax  /* incr */
        lock
        xaddl   %eax, (%edx)
        ret
