#include "sandbox_impl.h"
#include "syscall_table.h"

namespace playground {

void Sandbox::createTrustedThread(int processFdPub, int cloneFdPub,
                                  SecureMem::Args* secureMem) {
  SecureMem::Args args                  = { { { { { 0 } } } } };
  args.self                             = &args;
  args.newSecureMem                     = secureMem;
  args.processFdPub                     = processFdPub;
  args.cloneFdPub                       = cloneFdPub;
#if defined(__x86_64__)
  asm volatile(
      "push %%rbx\n"
      "push %%rbp\n"
      "mov  %0, %%rbp\n"           // %rbp = args
      "xor  %%rbx, %%rbx\n"        // initial sequence number
      "lea  999f(%%rip), %%r15\n"  // continue in same thread
      "jmp  19f\n"                 // create trusted thread

      // TODO(markus): Coalesce the read() operations by reading into a bigger
      // buffer.

      // Parameters:
      //   *%fs: secure memory region
      //         the page following this one contains the scratch space
      //   %r13: thread's side of threadFd
      //   %r15: processFdPub

      // Local variables:
      //   %rbx: sequence number for trusted calls

      // Temporary variables:
      //   %r9: system call number
      //  %rbp: secure memory of previous thread

      // Layout of secure shared memory region (c.f. securemem.h):
      //   0x00: pointer to the secure shared memory region (i.e. self)
      //   0x08: sequence number; must match %rbx
      //   0x10: system call number; passed to syscall in %rax
      //   0x18: first argument; passed to syscall in %rdi
      //   0x20: second argument; passed to syscall in %rsi
      //   0x28: third argument; passed to syscall in %rdx
      //   0x30: fourth argument; passed to syscall in %r10
      //   0x38: fifth argument; passed to syscall in %r8
      //   0x40: sixth argument; passed to syscall in %r9
      //   0x48: stored return address for clone() system call
      //   0x50: stored %rbp value for clone() system call
      //   0x58: stored %rbx value for clone() system call
      //   0x60: stored %rcx value for clone() system call
      //   0x68: stored %rdx value for clone() system call
      //   0x70: stored %rsi value for clone() system call
      //   0x78: stored %rdi value for clone() system call
      //   0x80: stored %r8 value for clone() system call
      //   0x88: stored %r9 value for clone() system call
      //   0x90: stored %r10 value for clone() system call
      //   0x98: stored %r11 value for clone() system call
      //   0xA0: stored %r12 value for clone() system call
      //   0xA8: stored %r13 value for clone() system call
      //   0xB0: stored %r14 value for clone() system call
      //   0xB8: stored %r15 value for clone() system call
      //   0xC0: new shared memory for clone()
      //   0xC8: processFdPub for talking to trusted process
      //   0xCC: cloneFdPub for talking to trusted process
      //   0xD0: set to non-zero, if in debugging mode
      //   0xD4: most recent SHM id returned by shmget(IPC_PRIVATE)
      //   0xD8: cookie assigned to us by the trusted process (TLS_COOKIE)
      //   0xE0: thread id (TLS_TID)
      //   0xE8: threadFdPub (TLS_THREAD_FD)
      //   0x200-0x1000: securely passed verified file name(s)

      // Layout of (untrusted) scratch space:
      //   0x00: syscall number; passed in %rax
      //   0x04: first argument; passed in %rdi
      //   0x0C: second argument; passed in %rsi
      //   0x14: third argument; passed in %rdx
      //   0x1C: fourth argument; passed in %r10
      //   0x24: fifth argument; passed in %r8
      //   0x2C: sixth argument; passed in %r9
      //   0x34: return value
      //   0x3C: RDTSCP result (%eax)
      //   0x40: RDTSCP result (%edx)
      //   0x44: RDTSCP result (%ecx)

      // We use the %fs register for accessing the secure read-only page, and
      // the untrusted scratch space immediately following it. The segment
      // register and the local descriptor table is set up by passing
      // appropriate arguments to clone().

    "0:xor  %%rsp, %%rsp\n"
      "mov  $2, %%ebx\n"           // %rbx  = initial sequence number

      // Read request from untrusted thread, or from trusted process. In either
      // case, the data that we read has to be considered untrusted.
      // read(threadFd, &scratch, 4)
    "1:xor  %%rax, %%rax\n"        // NR_read
      "mov  %%r13, %%rdi\n"        // fd  = threadFd
      "mov  %%fs:0x0, %%rsi\n"
      "add  $0x1000, %%rsi\n"      // buf = &scratch
      "mov  $4, %%edx\n"           // len = 4
    "2:syscall\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   2b\n"
      "cmp  %%rdx, %%rax\n"
      "jnz  25f\n"                 // exit process

      // Retrieve system call number. It is crucial that we only dereference
      // %fs:0x1000 exactly once. Afterwards, memory becomes untrusted and
      // we must use the value that we have read the first time.
      "mov  0(%%rsi), %%eax\n"

      // If syscall number is -1, execute an unlocked system call from the
      // secure memory area
      "cmp  $-1, %%eax\n"
      "jnz  5f\n"
    "3:cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "mov  %%fs:0x10, %%rax\n"
      "mov  %%fs:0x18, %%rdi\n"
      "mov  %%fs:0x20, %%rsi\n"
      "mov  %%fs:0x28, %%rdx\n"
      "mov  %%fs:0x30, %%r10\n"
      "mov  %%fs:0x38, %%r8\n"
      "mov  %%fs:0x40, %%r9\n"
      "cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%rbx\n"

      // shmget() gets some special treatment. Whenever we return from this
      // system call, we remember the most recently returned SysV shm id.
      "cmp  $29, %%eax\n"          // NR_shmget
      "jnz  4f\n"
      "syscall\n"
      "mov  %%rax, %%r8\n"
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%edi\n"          // flags = SIGCHLD
      "mov  $1, %%esi\n"           // stack = 1
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25f\n"                 // exit process
      "mov  %%rax, %%rdi\n"
      "jnz  7f\n"                  // wait for child, then return result
      "mov  %%fs:0x0, %%rdi\n"     // start = secure_mem
      "mov  $4096, %%esi\n"        // len   = 4096
      "mov  $3, %%edx\n"           // prot  = PROT_READ | PROT_WRITE
      "mov  $10, %%eax\n"          // NR_mprotect
      "syscall\n"
      "mov  %%r8d, 0xD4(%%rdi)\n"  // set most recently returned SysV shm id
      "xor  %%rdi, %%rdi\n"
      "jmp  26f\n"                 // exit program, no message
    "4:syscall\n"
      "jmp  14f\n"                 // return result

      // If syscall number is -2, execute locked system call from the
      // secure memory area
    "5:jg   11f\n"
      "cmp  $-2, %%eax\n"
      "jnz  8f\n"
      "cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "mov  %%fs:0x10, %%rax\n"
      "mov  %%fs:0x18, %%rdi\n"
      "mov  %%fs:0x20, %%rsi\n"
      "mov  %%fs:0x28, %%rdx\n"
      "mov  %%fs:0x30, %%r10\n"
      "mov  %%fs:0x38, %%r8\n"
      "mov  %%fs:0x40, %%r9\n"
      "cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process

      // clone() has unusual calling conventions and must be handled specially
      "cmp  $56, %%rax\n"          // NR_clone
      "jz   18f\n"

      // exit() terminates trusted thread
      "cmp  $60, %%eax\n"          // NR_exit
      "jz   17f\n"

      // Perform requested system call
      "syscall\n"

      // Unlock mutex
    "6:cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%rbx\n"
      "mov  %%rax, %%r8\n"
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25f\n"                 // exit process
      "jz   22f\n"                 // unlock and exit
      "mov  %%rax, %%rdi\n"
    "7:xor  %%rsi, %%rsi\n"
      "xor  %%rdx, %%rdx\n"
      "xor  %%r10, %%r10\n"
      "mov  $61, %%eax\n"          // NR_wait4
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   7b\n"
      "mov  %%r8, %%rax\n"
      "jmp  14f\n"                 // return result

      // If syscall number is -3, read the time stamp counter
    "8:cmp  $-3, %%eax\n"
      "jnz  9f\n"
      "rdtsc\n"                    // sets %edx:%eax
      "xor  %%rcx, %%rcx\n"
      "jmp  10f\n"
    "9:cmp  $-4, %%eax\n"
      "jnz  11f\n"
      "rdtscp\n"                   // sets %edx:%eax and %ecx
   "10:add  $0x3C, %%rsi\n"
      "mov  %%eax, 0(%%rsi)\n"
      "mov  %%edx, 4(%%rsi)\n"
      "mov  %%ecx, 8(%%rsi)\n"
      "mov  $12, %%edx\n"
      "jmp  15f\n"                 // return result

      // Check in syscallTable whether this system call is unrestricted
   "11:mov  %%rax, %%r9\n"
      #ifndef NDEBUG
      "cmpw $0, %%fs:0xD0\n"       // debug mode
      "jnz  12f\n"
      #endif
      "cmp  playground$maxSyscall(%%rip), %%eax\n"
      "ja   25f\n"                 // exit process
      "shl  $4, %%rax\n"
      "lea  playground$syscallTable(%%rip), %%rdi\n"
      "add  %%rdi, %%rax\n"
      "mov  0(%%rax), %%rax\n"
      "cmp  $1, %%rax\n"
      "jne  25f\n"                 // exit process

      // Default behavior for unrestricted system calls is to just execute
      // them. Read the remaining arguments first.
   "12:mov  %%rsi, %%r8\n"
      "xor  %%rax, %%rax\n"        // NR_read
      "mov  %%r13, %%rdi\n"        // fd  = threadFd
      "add  $4, %%rsi\n"           // buf = &scratch + 4
      "mov  $48, %%edx\n"          // len = 6*sizeof(void *)
   "13:syscall\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   13b\n"
      "cmp  %%rdx, %%rax\n"
      "jnz  25f\n"                 // exit process
      "mov  %%r9, %%rax\n"
      "mov  0x04(%%r8), %%rdi\n"
      "mov  0x0C(%%r8), %%rsi\n"
      "mov  0x14(%%r8), %%rdx\n"
      "mov  0x1C(%%r8), %%r10\n"
      "mov  0x2C(%%r8), %%r9\n"
      "mov  0x24(%%r8), %%r8\n"
      "cmp  $231, %%rax\n"         // NR_exit_group
      "jz   26f\n"                 // exit program, no message
      "syscall\n"

      // Return result of system call to sandboxed thread
   "14:mov  %%fs:0x0, %%rsi\n"
      "add  $0x1034, %%rsi\n"      // buf   = &scratch + 52
      "mov  %%rax, (%%rsi)\n"
      "mov  $8, %%edx\n"           // len   = 8
   "15:mov  %%r13, %%rdi\n"        // fd    = threadFd
      "mov  $1, %%eax\n"           // NR_write
   "16:syscall\n"
      "cmp  %%rdx, %%rax\n"
      "jz   1b\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   16b\n"
      "jmp  25f\n"                 // exit process

      // NR_exit:
      // Exit trusted thread after cleaning up resources
   "17:mov  %%fs:0x0, %%rsi\n"
      "mov  0xE8(%%rsi), %%rdi\n"  // fd     = threadFdPub
      "mov  $3, %%eax\n"           // NR_close
      "syscall\n"
      "mov  %%rsi, %%rdi\n"        // start  = secure_mem
      "mov  $8192, %%esi\n"        // length = 4096
      "xor  %%rdx, %%rdx\n"        // prot   = PROT_NONE
      "mov  $10, %%eax\n"          // NR_mprotect
      "syscall\n"
      "mov  %%r13, %%rdi\n"        // fd     = threadFd
      "mov  $3, %%eax\n"           // NR_close
      "syscall\n"
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"
      "mov  %%rax, %%rdi\n"
      "test %%rax, %%rax\n"
      "jne  21f\n"                 // reap helper, exit thread
      "jmp  22f\n"                 // unlock mutex

      // NR_clone:
      // Original trusted thread calls clone() to create new nascent
      // thread. This thread is (typically) fully privileged and shares all
      // resources with the caller (i.e. the previous trusted thread),
      // and by extension it shares all resources with the sandbox'd
      // threads.
      // N.B. It is possible to make the thread creation code crash before
      // it releases seccomp privileges. This is generally OK, as it just
      // terminates the program. But if we ever support signal handling,
      // we have to be careful that the user cannot install a SIGSEGV
      // handler that gets executed with elevated privileges.
   "18:mov  %%fs:0x0, %%rbp\n"     // %rbp = old_shared_mem
      "syscall\n"                  // calls NR_clone
      "cmp  $-4095, %%rax\n"       // return codes -1..-4095 are errno values
      "jae  6b\n"
      "add  $2, %%rbx\n"
      "test %%rax, %%rax\n"
      "jne  14b\n"                 // return result

      // In nascent thread, now.
      "sub  $2, %%rbx\n"
      "xor  %%r15, %%r15\n"        // Request to return from clone() when done

      // Get thread id of nascent thread
   "19:mov  $186, %%eax\n"         // NR_gettid
      "syscall\n"
      "mov  %%rax, %%r14\n"

      // Nascent thread creates socketpair() for sending requests to
      // trusted thread.
      // We can create the filehandles on the stack. Filehandles are
      // always treated as untrusted.
      // socketpair(AF_UNIX, SOCK_STREAM, 0, fds)
      "push %%r15\n"
      "mov  $53, %%eax\n"          // NR_socketpair
      "mov  $1, %%edi\n"           // domain = AF_UNIX
      "mov  $1, %%esi\n"           // type = SOCK_STREAM
      "xor  %%rdx, %%rdx\n"        // protocol = 0
      "sub  $8, %%rsp\n"           // sv = %rsp
      "mov  %%rsp, %%r10\n"
      "syscall\n"
      "test %%rax, %%rax\n"
      "jz   27f\n"

      // If things went wrong, we don't have an (easy) way of signaling
      // the parent. For our purposes, it is sufficient to fail with a
      // fatal error.
      "jmp  25f\n"                 // exit process
   "20:mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25f\n"                 // exit process
      "jz   22f\n"                 // unlock and exit
      "mov  %%rax, %%rdi\n"
   "21:xor  %%rsi, %%rsi\n"
      "xor  %%rdx, %%rdx\n"
      "xor  %%r10, %%r10\n"
      "mov  $61, %%eax\n"          // NR_wait4
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   21b\n"
      "jmp  23f\n"                 // exit thread (no message)
   "22:lea  playground$syscall_mutex(%%rip), %%rdi\n"
      "mov  $4096, %%esi\n"
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "mov  $10, %%eax\n"          // NR_mprotect
      "syscall\n"
      "lock; addl $0x80000000, (%%rdi)\n"
      "jz   23f\n"                 // exit thread
      "mov  $1, %%edx\n"
      "mov  %%rdx, %%rsi\n"        // FUTEX_WAKE
      "mov  $202, %%eax\n"         // NR_futex
      "syscall\n"
   "23:mov  $60, %%eax\n"          // NR_exit
      "mov  $1, %%edi\n"           // status = 1
   "24:syscall\n"
   "25:mov  $1, %%eax\n"           // NR_write
      "mov  $2, %%edi\n"           // fd = stderr
      "lea  100f(%%rip), %%rsi\n"
      "mov  $101f-100f, %%edx\n"   // len = strlen(msg)
      "syscall\n"
      "mov  $1, %%edi\n"
   "26:mov  $231, %%eax\n"         // NR_exit_group
      "jmp  24b\n"

      // The first page is mapped read-only for use as securely shared memory
   "27:mov  0xC0(%%rbp), %%r12\n"  // %r12 = secure shared memory
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process
      "mov  $10, %%eax\n"          // NR_mprotect
      "mov  %%r12, %%rdi\n"        // addr = secure_mem
      "mov  $4096, %%esi\n"        // len  = 4096
      "mov  $1, %%edx\n"           // prot = PROT_READ
      "syscall\n"

      // The second page is used as scratch space by the trusted thread.
      // Make it writable.
      "mov  $10, %%eax\n"          // NR_mprotect
      "add  $4096, %%rdi\n"        // addr = secure_mem + 4096
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "syscall\n"

      // Call clone() to create new trusted thread().
      // clone(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|
      //       CLONE_SYSVSEM|CLONE_UNTRACED|CLONE_SETTLS, stack, NULL, NULL,
      //       tls)
      "mov  4(%%rsp), %%r13d\n"    // %r13  = threadFd
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $0x8D0F00, %%edi\n"    // flags = VM|FS|FILES|SIGH|THR|SYSV|UTR|TLS
      "mov  $1, %%rsi\n"           // stack = 1
      "mov  %%r12, %%r8\n"         // tls   = new_secure_mem
      "mov  0xC8(%%rbp), %%r15d\n" // %r15  = processFdPub
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25b\n"                 // exit process
      "jz   0b\n"                  // invoke trustedThreadFnc()

      // Done creating trusted thread. We can now get ready to return to caller
      "mov  0(%%rsp), %%r9d\n"     // %r9 = threadFdPub
      "add  $8, %%rsp\n"

      // Set up thread local storage with information on how to talk to
      // trusted thread and trusted process.
      "lea  0xD8(%%r12), %%rsi\n"  // args   = &secure_mem.TLS;
      "mov  $158, %%eax\n"         // NR_arch_prctl
      "mov  $0x1001, %%edi\n"      // option = ARCH_SET_GS
      "syscall\n"
      "cmp  $-4095, %%rax\n"       // return codes -1..-4095 are errno values
      "jae  20b\n"                 // exit thread, unlock global mutex

      // Check whether this is the initial thread, or a newly created one.
      // At startup we run the same code as when we create a new thread. At
      // the very top of this function, you will find that we push 999(%rip)
      // on the stack. That is the signal that we should return on the same
      // stack rather than return to where clone was called.
      "pop  %%r15\n"
      "test %%r15, %%r15\n"
      "jne  28f\n"

      // Returning from clone() into the newly created thread is special. We
      // cannot unroll the stack, as we just set up a new stack for this
      // thread. We have to explicitly restore CPU registers to the values
      // that they had when the program originally called clone().
      "sub  $0x80, %%rsp\n"        // redzone compensation
      "mov  0x48(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x50(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x58(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x60(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x68(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x70(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x78(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x80(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x88(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x90(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0x98(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0xA0(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0xA8(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0xB0(%%rbp), %%rax\n"
      "push %%rax\n"
      "mov  0xB8(%%rbp), %%rax\n"
      "push %%rax\n"
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process

      // Nascent thread launches a helper that doesn't share any of our
      // resources, except for pages mapped as MAP_SHARED.
      // clone(0, %rsp)
   "28:mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  %%rsp, %%rsi\n"        // stack = %rsp
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25b\n"                 // exit process
      "jne  29f\n"

      // Use sendmsg() to send to the trusted process the file handles for
      // communicating with the new trusted thread. We also send the address
      // of the secure memory area (for sanity checks) and the thread id.
      "mov  0xCC(%%rbp), %%edi\n"  // transport = Sandbox::cloneFdPub()
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process
      "mov  %%r9, %%rsi\n"         // fd0       = threadFdPub
      "mov  %%r13, %%rdx\n"        // fd1       = threadFd
      "push %%r14\n"               // threadId
      "mov  %%esi, 4(%%rsp)\n"     // threadFdPub
      "push %%r12\n"               // secure_mem
      "mov  %%rsp, %%rcx\n"        // buf       = &data
      "mov  $16, %%r8\n"           // len       = sizeof(void*) + 2*sizeof(int)
      "call playground$sendFd\n"

      // Release syscall_mutex_. This signals the trusted process that
      // it can write into the original thread's secure memory again.
      "mov  $10, %%eax\n"          // NR_mprotect
      "lea  playground$syscall_mutex(%%rip), %%rdi\n"
      "mov  $4096, %%esi\n"
      "mov  $3, %%edx\n"           // PROT_READ | PROT_WRITE
      "syscall\n"
      "lock; addl $0x80000000, (%%rdi)\n"
      "jz   26b\n"                 // exit process (no error message)
      "mov  $1, %%edx\n"
      "mov  %%rdx, %%rsi\n"        // FUTEX_WAKE
      "mov  $202, %%eax\n"         // NR_futex
      "syscall\n"
      "jmp  26b\n"                 // exit process (no error message)

      // Reap helper
   "29:mov  %%rax, %%rdi\n"
   "30:xor  %%rsi, %%rsi\n"
      "xor  %%rdx, %%rdx\n"
      "xor  %%r10, %%r10\n"
      "mov  $61, %%eax\n"          // NR_wait4
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   30\n"

      // Release privileges by entering seccomp mode.
      "mov  $157, %%eax\n"         // NR_prctl
      "mov  $22, %%edi\n"          // PR_SET_SECCOMP
      "mov  $1, %%esi\n"
      "syscall\n"
      "test %%rax, %%rax\n"
      "jnz  25b\n"                 // exit process

      // Back in the newly created sandboxed thread, wait for trusted process
      // to receive request. It is possible for an attacker to make us
      // continue even before the trusted process is done. This is OK. It'll
      // result in us putting stale values into the new thread's TLS. But that
      // data is considered untrusted anyway.
      "push %%rax\n"
      "mov  $1, %%edx\n"           // len       = 1
      "mov  %%rsp, %%rsi\n"        // buf       = %rsp
      "mov  %%r9, %%rdi\n"         // fd        = threadFdPub
   "31:xor  %%rax, %%rax\n"        // NR_read
      "syscall\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   31b\n"
      "cmp  %%rdx, %%rax\n"
      "jne  25b\n"                 // exit process
      "pop  %%rax\n"

      // Return to caller. We are in the new thread, now.
      "xor  %%rax, %%rax\n"
      "test %%r15, %%r15\n"

      // Returning to createTrustedThread()
      "jz   32f\n"
      "jmp  *%%r15\n"

      // Returning to the place where clone() had been called
   "32:pop  %%r15\n"
      "pop  %%r14\n"
      "pop  %%r13\n"
      "pop  %%r12\n"
      "pop  %%r11\n"
      "pop  %%r10\n"
      "pop  %%r9\n"
      "pop  %%r8\n"
      "pop  %%rdi\n"
      "pop  %%rsi\n"
      "pop  %%rdx\n"
      "pop  %%rcx\n"
      "pop  %%rbx\n"
      "pop  %%rbp\n"
      "ret\n"

      ".pushsection \".rodata\"\n"
  "100:.ascii \"Sandbox violation detected, program aborted\\n\"\n"
  "101:\n"
      ".popsection\n"

  "999:pop  %%rbp\n"
      "pop  %%rbx\n"
      :
      : "g"(&args)
      : "rax", "rcx", "rdx", "rdi", "rsi", "r8", "r9", "r10", "r11", "r12",
        "r13", "r14", "r15", "rsp", "memory"
#elif defined(__i386__)
  struct user_desc u;
  u.entry_number    = (typeof u.entry_number)-1;
  u.base_addr       = 0;
  u.limit           = 0xfffff;
  u.seg_32bit       = 1;
  u.contents        = 0;
  u.read_exec_only  = 0;
  u.limit_in_pages  = 1;
  u.seg_not_present = 0;
  u.useable         = 1;
  SysCalls sys;
  if (sys.set_thread_area(&u) < 0) {
    die("Cannot set up thread local storage");
  }
  asm volatile("movw %w0, %%fs"
      :
      : "q"(8*u.entry_number+3));
  asm volatile(
      "push %%ebx\n"
      "push %%ebp\n"
      "movd %0, %%mm6\n"           // %mm6 = args
      "lea  999f, %%ebx\n"         // continue in same thread
      "movd %%ebx, %%mm3\n"
      "xor  %%ebx, %%ebx\n"        // initial sequence number
      "movd %%ebx, %%mm2\n"
      "jmp  19f\n"                 // create trusted thread

      // TODO(markus): Coalesce the read() operations by reading into a bigger
      // buffer.

      // Parameters:
      //   %mm5: secure memory region
      //         the page following this one contains the scratch space
      //   %mm0: thread's side of threadFd
      //   %mm1: processFdPub
      //   %mm3: return address after creation of new trusted thread

      // Local variables:
      //   %mm2: sequence number for trusted calls
      //   %mm4: thread id

      // Temporary variables:
      //   %ebp: system call number
      //   %mm6: secure memory of previous thread
      //   %mm7: temporary variable for spilling data

      // Layout of secure shared memory region (c.f. securemem.h):
      //   0x00: pointer to the secure shared memory region (i.e. self)
      //   0x04: sequence number; must match %mm2
      //   0x08: system call number; passed to syscall in %eax
      //   0x0C: first argument; passed to syscall in %ebx
      //   0x10: second argument; passed to syscall in %ecx
      //   0x14: third argument; passed to syscall in %edx
      //   0x18: fourth argument; passed to syscall in %esi
      //   0x1C: fifth argument; passed to syscall in %edi
      //   0x20: sixth argument; passed to syscall in %ebp
      //   0x24: stored return address for clone() system call
      //   0x28: second stored return address for clone() system call
      //   0x2C: stored %ebp value for clone() system call
      //   0x30: stored %edi value for clone() system call
      //   0x34: stored %esi value for clone() system call
      //   0x38: stored %edx value for clone() system call
      //   0x3C: stored %ecx value for clone() system call
      //   0x40: stored %ebx value for clone() system call
      //   0x44: new shared memory for clone()
      //   0x48: processFdPub for talking to trusted process
      //   0x4C: cloneFdPub for talking to trusted process
      //   0x50: set to non-zero, if in debugging mode
      //   0x54: most recent SHM id returned by shmget(IPC_PRIVATE)
      //   0x58: cookie assigned to us by the trusted process (TLS_COOKIE)
      //   0x60: thread id (TLS_TID)
      //   0x68: threadFdPub (TLS_THREAD_FD)
      //   0x200-0x1000: securely passed verified file name(s)

      // Layout of (untrusted) scratch space:
      //   0x00: syscall number; passed in %eax
      //   0x04: first argument; passed in %ebx
      //   0x08: second argument; passed in %ecx
      //   0x0C: third argument; passed in %edx
      //   0x10: fourth argument; passed in %esi
      //   0x14: fifth argument; passed in %edi
      //   0x18: sixth argument; passed in %ebp
      //   0x1C: return value
      //   0x20: RDTSCP result (%eax)
      //   0x24: RDTSCP result (%edx)
      //   0x28: RDTSCP result (%ecx)

    "0:xor  %%esp, %%esp\n"
      "mov  $2, %%eax\n"           // %mm2 = initial sequence number
      "movd %%eax, %%mm2\n"

      // Read request from untrusted thread, or from trusted process. In either
      // case, the data that we read has to be considered untrusted.
      // read(threadFd, &scratch, 4)
    "1:mov  $3, %%eax\n"           // NR_read
      "movd %%mm0, %%ebx\n"        // fd  = threadFd
      "movd %%mm5, %%ecx\n"
      "add  $0x1000, %%ecx\n"      // buf = &scratch
      "mov  $4, %%edx\n"           // len = 4
    "2:int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   2b\n"
      "cmp  %%edx, %%eax\n"
      "jnz  25f\n"                 // exit process

      // Retrieve system call number. It is crucial that we only dereference
      // 0x1000(%mm5) exactly once. Afterwards, memory becomes untrusted and
      // we must use the value that we have read the first time.
      "mov  0(%%ecx), %%eax\n"

      // If syscall number is -1, execute an unlocked system call from the
      // secure memory area
      "cmp  $-1, %%eax\n"
      "jnz  5f\n"
    "3:movd %%mm2, %%ebp\n"
      "cmp  %%ebp, 0x4-0x1000(%%ecx)\n"
      "jne  25f\n"                 // exit process
      "mov  0x08-0x1000(%%ecx), %%eax\n"
      "mov  0x0C-0x1000(%%ecx), %%ebx\n"
      "mov  0x14-0x1000(%%ecx), %%edx\n"
      "mov  0x18-0x1000(%%ecx), %%esi\n"
      "mov  0x1C-0x1000(%%ecx), %%edi\n"
      "mov  0x20-0x1000(%%ecx), %%ebp\n"
      "mov  0x10-0x1000(%%ecx), %%ecx\n"
      "movd %%edi, %%mm4\n"
      "movd %%ebp, %%mm7\n"
      "movd %%mm2, %%ebp\n"
      "movd %%mm5, %%edi\n"
      "cmp  %%ebp, 4(%%edi)\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%ebp\n"
      "movd %%ebp, %%mm2\n"
      "movd %%mm4, %%edi\n"
      "movd %%mm7, %%ebp\n"

      // shmget() gets some special treatment. Whenever we return from this
      // system call, we remember the most recently returned SysV shm id.
      "cmp  $117, %%eax\n"         // NR_ipc
      "jnz  4f\n"
      "cmp  $23, %%ebx\n"          // shmget()
      "jnz  4f\n"
      "int  $0x80\n"
      "mov  %%eax, %%ebp\n"
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25f\n"                 // exit process
      "mov  %%eax, %%ebx\n"
      "jnz  7f\n"                  // wait for child, then return result
      "movd %%mm5, %%ebx\n"        // start = secure_mem
      "mov  $4096, %%ecx\n"        // len   = 4096
      "mov  $3, %%edx\n"           // prot  = PROT_READ | PROT_WRITE
      "mov  $125, %%eax\n"         // NR_mprotect
      "int  $0x80\n"
      "mov  %%ebp, 0x54(%%ebx)\n"  // set most recently returned SysV shm id
      "xor  %%ebx, %%ebx\n"
      "jmp  26f\n"                 // exit program, no message
    "4:int  $0x80\n"
      "jmp  14f\n"                 // return result

      // If syscall number is -2, execute locked system call from the
      // secure memory area
    "5:jg   11f\n"
      "cmp  $-2, %%eax\n"
      "jnz  8f\n"
      "movd %%mm2, %%ebp\n"
      "cmp  %%ebp, 0x4-0x1000(%%ecx)\n"
      "jne  25f\n"                 // exit process
      "mov  0x08-0x1000(%%ecx), %%eax\n"
      "mov  0x0C-0x1000(%%ecx), %%ebx\n"
      "mov  0x14-0x1000(%%ecx), %%edx\n"
      "mov  0x18-0x1000(%%ecx), %%esi\n"
      "mov  0x1C-0x1000(%%ecx), %%edi\n"
      "mov  0x20-0x1000(%%ecx), %%ebp\n"
      "mov  0x10-0x1000(%%ecx), %%ecx\n"
      "movd %%edi, %%mm4\n"
      "movd %%ebp, %%mm7\n"
      "movd %%mm2, %%ebp\n"
      "movd %%mm5, %%edi\n"
      "cmp  %%ebp, 4(%%edi)\n"
      "jne  25f\n"                 // exit process

      // clone() has unusual calling conventions and must be handled specially
      "cmp  $120, %%eax\n"         // NR_clone
      "jz   18f\n"

      // exit() terminates trusted thread
      "cmp  $1, %%eax\n"           // NR_exit
      "jz   17f\n"

      // Perform requested system call
      "movd %%mm4, %%edi\n"
      "movd %%mm7, %%ebp\n"
      "int  $0x80\n"

      // Unlock mutex
    "6:movd %%mm2, %%ebp\n"
      "movd %%mm5, %%edi\n"
      "cmp  %%ebp, 4(%%edi)\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%ebp\n"
      "movd %%ebp, %%mm2\n"
      "mov  %%eax, %%ebp\n"
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25f\n"                 // exit process
      "jz   22f\n"                 // unlock and exit
      "mov  %%eax, %%ebx\n"
    "7:xor  %%ecx, %%ecx\n"
      "xor  %%edx, %%edx\n"
      "mov  $7, %%eax\n"           // NR_waitpid
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   6\n"
      "mov  %%ebp, %%eax\n"
      "jmp  14f\n"                 // return result

      // If syscall number is -3, read the time stamp counter
    "8:cmp  $-3, %%eax\n"
      "jnz  9f\n"
      "rdtsc\n"                    // sets %edx:%eax
      "xor  %%ecx, %%ecx\n"
      "jmp  10f\n"
    "9:cmp  $-4, %%eax\n"
      "jnz  11f\n"
      "rdtscp\n"                   // sets %edx:%eax and %ecx
   "10:movd %%mm5, %%ebx\n"
      "add  $0x1020, %%ebx\n"
      "mov  %%eax, 0(%%ebx)\n"
      "mov  %%edx, 4(%%ebx)\n"
      "mov  %%ecx, 8(%%ebx)\n"
      "mov  %%ebx, %%ecx\n"
      "mov  $12, %%edx\n"
      "jmp  15f\n"                 // return result

      // Check in syscallTable whether this system call is unrestricted
   "11:mov  %%eax, %%ebp\n"
      #ifndef NDEBUG
      "cmpw $0, 0x50-0x1000(%%ecx)\n"
      "jnz  12f\n"                 // debug mode
      #endif
      "cmp  playground$maxSyscall, %%eax\n"
      "ja   25f\n"                 // exit process
      "shl  $3, %%eax\n"
      "add  $playground$syscallTable, %%eax\n"
      "mov  0(%%eax), %%eax\n"
      "cmp  $1, %%eax\n"
      "jne  25f\n"                 // exit process

      // Default behavior for unrestricted system calls is to just execute
      // them. Read the remaining arguments first.
   "12:mov  $3, %%eax\n"           // NR_read
      "movd %%mm0, %%ebx\n"        // fd  = threadFd
      "add  $4, %%ecx\n"           // buf = &scratch + 4
      "mov  $24, %%edx\n"          // len = 6*sizeof(void *)
   "13:int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   13b\n"
      "cmp  %%edx, %%eax\n"
      "jnz  25f\n"                 // exit process
      "mov  %%ebp, %%eax\n"
      "mov  0x00(%%ecx), %%ebx\n"
      "mov  0x08(%%ecx), %%edx\n"
      "mov  0x0C(%%ecx), %%esi\n"
      "mov  0x10(%%ecx), %%edi\n"
      "mov  0x14(%%ecx), %%ebp\n"
      "mov  0x04(%%ecx), %%ecx\n"
      "cmp  $252, %%eax\n"         // NR_exit_group
      "jz   26f\n"                 // exit program, no message
      "int  $0x80\n"

      // Return result of system call to sandboxed thread
   "14:movd %%mm5, %%ecx\n"
      "add  $0x101C, %%ecx\n"      // buf   = &scratch + 28
      "mov  %%eax, (%%ecx)\n"
      "mov  $4, %%edx\n"           // len   = 4
   "15:movd %%mm0, %%ebx\n"        // fd    = threadFd
      "mov  $4, %%eax\n"           // NR_write
   "16:int  $0x80\n"
      "cmp  %%edx, %%eax\n"
      "jz   1b\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   16b\n"
      "jmp  25f\n"                 // exit process

      // NR_exit:
      // Exit trusted thread after cleaning up resources
   "17:mov  %%edi, %%ecx\n"
      "mov  0x68(%%ecx), %%ebx\n"  // fd     = threadFdPub
      "mov  $6, %%eax\n"           // NR_close
      "int  $0x80\n"
      "mov  %%ecx, %%ebx\n"        // start  = secure_mem
      "mov  $8192, %%ecx\n"        // length = 4096
      "xor  %%edx, %%edx\n"        // prot   = PROT_NONE
      "mov  $125, %%eax\n"         // NR_mprotect
      "int  $0x80\n"
      "movd %%mm0, %%ebx\n"        // fd     = threadFd
      "mov  $6, %%eax\n"           // NR_close
      "int  $0x80\n"
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "mov  %%eax, %%ebx\n"
      "test %%eax, %%eax\n"
      "jne  21f\n"                 // reap helper, exit thread
      "jmp  22f\n"                 // unlock mutex

      // NR_clone:
      // Original trusted thread calls clone() to create new nascent
      // thread. This thread is (typically) fully privileged and shares all
      // resources with the caller (i.e. the previous trusted thread),
      // and by extension it shares all resources with the sandbox'd
      // threads.
      // N.B. It is possible to make the thread creation code crash before
      // it releases seccomp privileges. This is generally OK, as it just
      // terminates the program. But if we ever support signal handling,
      // we have to be careful that the user cannot install a SIGSEGV
      // handler that gets executed with elevated privileges.
   "18:movd %%edi, %%mm6\n"        // %mm6 = old_shared_mem
      "movd %%mm4, %%edi\n"
      "movd %%mm7, %%ebp\n"
      "int  $0x80\n"               // calls NR_clone
      "cmp  $-4095, %%eax\n"       // return codes -1..-4095 are errno values
      "jae  6b\n"
      "movd %%mm2, %%edi\n"
      "add  $2, %%edi\n"
      "movd %%edi, %%mm2\n"
      "test %%eax, %%eax\n"
      "jne  14b\n"                 // return result

      // In nascent thread, now.
      "sub  $2, %%edi\n"
      "movd %%edi, %%mm2\n"
      "movd %%eax, %%mm3\n"        // Request to return from clone() when done

      // Get thread id of nascent thread
   "19:mov  $224, %%eax\n"         // NR_gettid
      "int  $0x80\n"
      "movd %%eax, %%mm4\n"

      // Nascent thread creates socketpair() for sending requests to
      // trusted thread.
      // We can create the filehandles on the stack. Filehandles are
      // always treated as untrusted.
      // socketpair(AF_UNIX, SOCK_STREAM, 0, fds)
      "mov  $102, %%eax\n"         // NR_socketcall
      "mov  $8, %%ebx\n"           // socketpair
      "sub  $8, %%esp\n"           // sv = %rsp
      "push %%esp\n"
      "xor  %%ecx, %%ecx\n"        // protocol = 0
      "push %%ecx\n"
      "mov  $1, %%ecx\n"           // type     = SOCK_STREAM
      "push %%ecx\n"
      "push %%ecx\n"               // domain   = AF_UNIX
      "mov  %%esp, %%ecx\n"
      "int  $0x80\n"
      "add  $0x10, %%esp\n"
      "test %%eax, %%eax\n"
      "jz   27f\n"

      // If things went wrong, we don't have an (easy) way of signaling
      // the parent. For our purposes, it is sufficient to fail with a
      // fatal error.
      "jmp  25f\n"                 // exit process
   "20:mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25f\n"                 // exit process
      "jz   22f\n"                 // unlock and exit
      "mov  %%eax, %%ebx\n"
   "21:xor  %%ecx, %%ecx\n"
      "xor  %%edx, %%edx\n"
      "mov  $7, %%eax\n"           // NR_waitpid
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   21b\n"
      "jmp  23f\n"                 // exit thread (no message)
   "22:lea  playground$syscall_mutex, %%ebx\n"
      "mov  $4096, %%ecx\n"
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "mov  $125, %%eax\n"         // NR_mprotect
      "int  $0x80\n"
      "lock; addl $0x80000000, (%%ebx)\n"
      "jz   23f\n"                 // exit thread
      "mov  $1, %%edx\n"
      "mov  %%edx, %%ecx\n"        // FUTEX_WAKE
      "mov  $240, %%eax\n"         // NR_futex
      "int  $0x80\n"
   "23:mov  $1, %%eax\n"           // NR_exit
      "mov  $1, %%ebx\n"           // status = 1
   "24:int  $0x80\n"
   "25:mov  $4, %%eax\n"           // NR_write
      "mov  $2, %%ebx\n"           // fd = stderr
      "lea  100f, %%ecx\n"
      "mov  $101f-100f, %%edx\n"   // len = strlen(msg)
      "int  $0x80\n"
      "mov  $1, %%ebx\n"
   "26:mov  $252, %%eax\n"         // NR_exit_group
      "jmp  24b\n"

      // The first page is mapped read-only for use as securely shared memory
   "27:movd %%mm6, %%ebp\n"
      "mov  0x44(%%ebp), %%esi\n"
      "movd %%esi, %%mm5\n"        // %mm5 = secure shared memory
      "movd %%mm2, %%edi\n"
      "cmp  %%edi, 4(%%ebp)\n"
      "jne  25b\n"                 // exit process
      "mov  $125, %%eax\n"         // NR_mprotect
      "mov  %%esi, %%ebx\n"
      "mov  $4096, %%ecx\n"        // len  = 4096
      "mov  $1, %%edx\n"           // prot = PROT_READ
      "int  $0x80\n"

      // The second page is used as scratch space by the trusted thread.
      // Make it writable.
      "mov  $125, %%eax\n"         // NR_mprotect
      "add  $4096, %%ebx\n"        // addr = secure_mem + 4096
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "int  $0x80\n"

      // Call clone() to create new trusted thread().
      // clone(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|
      //       CLONE_SYSVSEM|CLONE_UNTRACED, stack, NULL, NULL, NULL)
      "mov  4(%%esp), %%eax\n"
      "movd %%eax, %%mm0\n"        // %mm0  = threadFd
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $0x850F00, %%ebx\n"    // flags = VM|FS|FILES|SIGH|THR|SYSV|UTR
      "mov  $1, %%ecx\n"           // stack = 1
      "movd 0x48(%%ebp), %%mm1\n"  // %mm1  = processFdPub
      "cmp  %%edi, 4(%%ebp)\n"
      "jne  25b\n"                 // exit process
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25b\n"                 // exit process
      "jz   0b\n"                  // invoke trustedThreadFnc()

      // Set up thread local storage
      "mov  $0x51, %%eax\n"        // seg_32bit, limit_in_pages, useable
      "push %%eax\n"
      "mov  $0xFFFFF, %%eax\n"     // limit
      "push %%eax\n"
      "add  $0x58, %%esi\n"
      "push %%esi\n"               // base_addr = &secure_mem.TLS
      "mov  %%fs, %%eax\n"
      "shr  $3, %%eax\n"
      "push %%eax\n"               // entry_number
      "mov  $243, %%eax\n"         // NR_set_thread_area
      "mov  %%esp, %%ebx\n"
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "jnz  25b\n"                 // exit process
      "add  $16, %%esp\n"

      // Done creating trusted thread. We can now get ready to return to caller
      "mov  0(%%esp), %%esi\n"     // %esi = threadFdPub
      "add  $8, %%esp\n"

      // Check whether this is the initial thread, or a newly created one.
      // At startup we run the same code as when we create a new thread. At
      // the very top of this function, you will find that we store 999(%rip)
      // in %%mm3. That is the signal that we should return on the same
      // stack rather than return to where clone was called.
      "movd %%mm3, %%eax\n"
      "test %%eax, %%eax\n"
      "jne  28f\n"

      // Returning from clone() into the newly created thread is special. We
      // cannot unroll the stack, as we just set up a new stack for this
      // thread. We have to explicitly restore CPU registers to the values
      // that they had when the program originally called clone().
      "mov  0x24(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x28(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x2C(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x30(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x34(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x38(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x3C(%%ebp), %%eax\n"
      "push %%eax\n"
      "mov  0x40(%%ebp), %%eax\n"
      "push %%eax\n"
      "cmp  %%edi, 4(%%ebp)\n"
      "jne  25b\n"                 // exit process

      // Nascent thread launches a helper that doesn't share any of our
      // resources, except for pages mapped as MAP_SHARED.
      // clone(0, %esp)
   "28:mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  %%esp, %%ecx\n"        // stack = %esp
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25b\n"                 // exit process
      "jne  29f\n"

      // Use sendmsg() to send to the trusted process the file handles for
      // communicating with the new trusted thread. We also send the address
      // of the secure memory area (for sanity checks) and the thread id.
      "push %%esi\n"               // threadFdPub
      "movd %%mm4, %%eax\n"        // threadId
      "push %%eax\n"
      "movd %%mm5, %%eax\n"        // secure_mem
      "push %%eax\n"
      "mov  %%esp, %%ebx\n"        // buf       = &data
      "mov  $12, %%eax\n"          // len       = sizeof(void*) + 2*sizeof(int)
      "push %%eax\n"
      "push %%ebx\n"
      "movd %%mm0, %%eax\n"        // fd1       = threadFd
      "push %%eax\n"
      "push %%esi\n"               // fd0       = threadFdPub
      "mov  0x4C(%%ebp), %%eax\n"  // transport = Sandbox::cloneFdPub()
      "cmp  %%edi, 4(%%ebp)\n"
      "jne  25b\n"                 // exit process
      "push %%eax\n"
      "call playground$sendFd\n"

      // Release syscall_mutex_. This signals the trusted process that
      // it can write into the original thread's secure memory again.
      "mov  $125, %%eax\n"         // NR_mprotect
      "lea  playground$syscall_mutex, %%ebx\n"
      "mov  $4096, %%ecx\n"
      "mov  $3, %%edx\n"           // PROT_READ | PROT_WRITE
      "int  $0x80\n"
      "lock; addl $0x80000000, (%%ebx)\n"
      "jz   26b\n"                 // exit process (no error message)
      "mov  $1, %%edx\n"
      "mov  %%edx, %%ecx\n"        // FUTEX_WAKE
      "mov  $240, %%eax\n"         // NR_futex
      "int  $0x80\n"
      "jmp  26b\n"                 // exit process (no error message)

      // Reap helper
   "29:mov  %%eax, %%ebx\n"
   "30:xor  %%ecx, %%ecx\n"
      "xor  %%edx, %%edx\n"
      "mov  $7, %%eax\n"           // NR_waitpid
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   30\n"

      // Release privileges by entering seccomp mode.
      "mov  $172, %%eax\n"         // NR_prctl
      "mov  $22, %%ebx\n"          // PR_SET_SECCOMP
      "mov  $1, %%ecx\n"
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "jnz  25b\n"                 // exit process

      // Back in the newly created sandboxed thread, wait for trusted process
      // to receive request. It is possible for an attacker to make us
      // continue even before the trusted process is done. This is OK. It'll
      // result in us putting stale values into the new thread's TLS. But that
      // data is considered untrusted anyway.
      "push %%eax\n"
      "mov  $1, %%edx\n"           // len       = 1
      "mov  %%esp, %%ecx\n"        // buf       = %rsp
      "mov  %%esi, %%ebx\n"        // fd        = threadFdPub
   "31:mov  $3, %%eax\n"           // NR_read
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   31b\n"
      "cmp  %%edx, %%eax\n"
      "jne  25b\n"                 // exit process
      "pop  %%eax\n"

      // Return to caller. We are in the new thread, now.
      "xor  %%eax, %%eax\n"
      "movd %%mm3, %%ebx\n"

      // Release MMX registers, so that they can be used for floating point
      // operations.
      "emms\n"

      // Returning to createTrustedThread()
      "test %%ebx, %%ebx\n"
      "jz   32f\n"
      "jmp  *%%ebx\n"

      // Returning to the place where clone() had been called
   "32:pop  %%ebx\n"
      "pop  %%ecx\n"
      "pop  %%edx\n"
      "pop  %%esi\n"
      "pop  %%edi\n"
      "pop  %%ebp\n"
      "ret\n"

      ".pushsection \".rodata\"\n"
  "100:.ascii \"Sandbox violation detected, program aborted\\n\"\n"
  "101:\n"
      ".popsection\n"

  "999:pop  %%ebp\n"
      "pop  %%ebx\n"
      :
      : "g"(&args)
      : "eax", "ecx", "edx", "edi", "esi", "esp", "memory"
#else
#error Unsupported target platform
#endif
);
}

} // namespace
