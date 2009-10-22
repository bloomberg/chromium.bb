#include "library.h"
#include "sandbox_impl.h"
#include "syscall_table.h"

namespace playground {

// Global variables
int                           Sandbox::pid_;
int                           Sandbox::processFdPub_;
int                           Sandbox::cloneFdPub_;
Sandbox::ProtectedMap         Sandbox::protectedMap_;
std::vector<SecureMem::Args*> Sandbox::secureMemPool_;


bool Sandbox::sendFd(int transport, int fd0, int fd1, const void* buf,
                     size_t len) {
  int fds[2], count                     = 0;
  if (fd0 >= 0) { fds[count++]          = fd0; }
  if (fd1 >= 0) { fds[count++]          = fd1; }
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct SysCalls::kernel_iovec  iov[2] = { { 0 } };
  struct SysCalls::kernel_msghdr msg    = { 0 };
  int dummy                             = 0;
  iov[0].iov_base                       = &dummy;
  iov[0].iov_len                        = sizeof(dummy);
  if (buf && len > 0) {
    iov[1].iov_base                     = const_cast<void *>(buf);
    iov[1].iov_len                      = len;
  }
  msg.msg_iov                           = iov;
  msg.msg_iovlen                        = (buf && len > 0) ? 2 : 1;
  msg.msg_control                       = cmsg_buf;
  msg.msg_controllen                    = CMSG_LEN(count*sizeof(int));
  struct cmsghdr *cmsg                  = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level                      = SOL_SOCKET;
  cmsg->cmsg_type                       = SCM_RIGHTS;
  cmsg->cmsg_len                        = CMSG_LEN(count*sizeof(int));
  memcpy(CMSG_DATA(cmsg), fds, count*sizeof(int));
  SysCalls sys;
  return NOINTR_SYS(sys.sendmsg(transport, &msg, 0)) ==
      (ssize_t)(sizeof(dummy) + ((buf && len > 0) ? len : 0));
}

bool Sandbox::getFd(int transport, int* fd0, int* fd1, void* buf, size_t*len) {
  int count                            = 0;
  int *err                             = NULL;
  if (fd0) {
    count++;
    err                                = fd0;
    *fd0                               = -1;
  }
  if (fd1) {
    if (!count++) {
      err                              = fd1;
    }
    *fd1                               = -1;
  }
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct SysCalls::kernel_iovec iov[2] = { { 0 } };
  struct SysCalls::kernel_msghdr msg   = { 0 };
  iov[0].iov_base                      = err;
  iov[0].iov_len                       = sizeof(int);
  if (buf && len && *len > 0) {
    iov[1].iov_base                    = buf;
    iov[1].iov_len                     = *len;
  }
  msg.msg_iov                          = iov;
  msg.msg_iovlen                       = (buf && len && *len > 0) ? 2 : 1;
  msg.msg_control                      = cmsg_buf;
  msg.msg_controllen                   = CMSG_LEN(count*sizeof(int));
  SysCalls sys;
  ssize_t bytes = NOINTR_SYS(sys.recvmsg(transport, &msg, 0));
  if (len) {
    *len                               = bytes > (int)sizeof(int) ?
                                           bytes - sizeof(int) : 0;
  }
  if (bytes != (ssize_t)(sizeof(int) + ((buf && len && *len > 0) ? *len : 0))){
    *err                               = bytes >= 0 ? 0 : -EBADF;
    return false;
  }
  if (*err) {
    // "err" is the first four bytes of the payload. If these are non-zero,
    // the sender on the other side of the socketpair sent us an errno value.
    // We don't expect to get any file handles in this case.
    return false;
  }
  struct cmsghdr *cmsg               = CMSG_FIRSTHDR(&msg);
  if ((msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) ||
      !cmsg                                    ||
      cmsg->cmsg_level != SOL_SOCKET           ||
      cmsg->cmsg_type  != SCM_RIGHTS           ||
      cmsg->cmsg_len   != CMSG_LEN(count*sizeof(int))) {
    *err                             = -EBADF;
    return false;
  }
  if (fd1) { *fd1 = ((int *)CMSG_DATA(cmsg))[--count]; }
  if (fd0) { *fd0 = ((int *)CMSG_DATA(cmsg))[--count]; }
  return true;
}

void Sandbox::setupSignalHandlers() {
  SysCalls sys;
  struct SysCalls::kernel_sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler_ = SIG_DFL;
  sys.sigaction(SIGCHLD, &sa, NULL);

  // Set up SEGV handler for dealing with RDTSC instructions
  sa.sa_handler_ = segv();
  sys.sigaction(SIGSEGV, &sa, NULL);

  // Block all asynchronous signals, except for SIGCHLD which needs to be
  // set to SIG_DFL for waitpid() to work.
  SysCalls::kernel_sigset_t mask;
  memset(&mask, 0xFF, sizeof(mask));
  mask.sig[0]   &= ~((1 << (SIGSEGV - 1)) | (1 << (SIGINT  - 1)) |
                     (1 << (SIGTERM - 1)) | (1 << (SIGQUIT - 1)) |
                     (1 << (SIGHUP  - 1)) | (1 << (SIGABRT - 1)) |
                     (1 << (SIGCHLD - 1)));
  sys.sigprocmask(SIG_SETMASK, &mask, 0);
}

void (*Sandbox::segv())(int signo) {
  void (*fnc)(int signo);
  asm volatile(
      "call 999f\n"
#if defined(__x86_64__)
      // Inspect instruction at the point where the segmentation fault
      // happened. If it is RDTSC, forward the request to the trusted
      // thread.
      "mov  $-3, %%r14\n"          // request for RDTSC
      "mov  0xB0(%%rsp), %%r15\n"  // %rip at time of segmentation fault
      "cmpw $0x310F, (%%r15)\n"    // RDTSC
      "jz   0f\n"
      "cmpw $0x010F, (%%r15)\n"    // RDTSCP
      "jnz  8f\n"
      "cmpb $0xF9, 2(%%r15)\n"
      "jnz  8f\n"
      "mov  $-4, %%r14\n"          // request for RDTSCP
    "0:"
#ifndef NDEBUG
      "lea  100f(%%rip), %%rdi\n"
      "call playground$debugMessage\n"
#endif
      "sub  $4, %%rsp\n"
      "push %%r14\n"
      "mov  %%gs:16, %%edi\n"      // fd  = threadFdPub
      "mov  %%rsp, %%rsi\n"        // buf = %esp
      "mov  $4, %%edx\n"           // len = sizeof(int)
    "1:mov  $1, %%eax\n"           // NR_write
      "syscall\n"
      "cmp  %%rax, %%rdx\n"
      "jz   5f\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   1b\n"
    "2:add  $12, %%rsp\n"
      "movq $0, 0x98(%%rsp)\n"     // %rax at time of segmentation fault
      "movq $0, 0x90(%%rsp)\n"     // %rdx at time of segmentation fault
      "cmpw $0x310F, (%%r15)\n"    // RDTSC
      "jz   3f\n"
      "movq $0, 0xA0(%%rsp)\n"     // %rcx at time of segmentation fault
    "3:addq $2, 0xB0(%%rsp)\n"     // %rip at time of segmentation fault
      "cmpw $0x010F, (%%r15)\n"    // RDTSC
      "jnz  4f\n"
      "addq $1, 0xB0(%%rsp)\n"     // %rip at time of segmentation fault
    "4:ret\n"
    "5:mov  $12, %%edx\n"          // len = 3*sizeof(int)
    "6:mov  $0, %%eax\n"           // NR_read
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   6b\n"
      "cmp  %%rax, %%rdx\n"
      "jnz  2b\n"
      "mov  0(%%rsp), %%eax\n"
      "mov  4(%%rsp), %%edx\n"
      "mov  8(%%rsp), %%ecx\n"
      "add  $12, %%rsp\n"
      "mov  %%rdx, 0x90(%%rsp)\n"  // %rdx at time of segmentation fault
      "cmpw $0x310F, (%%r15)\n"    // RDTSC
      "jz   7f\n"
      "mov  %%rcx, 0xA0(%%rsp)\n"  // %rcx at time of segmentation fault
    "7:mov  %%rax, 0x98(%%rsp)\n"  // %rax at time of segmentation fault
      "jmp  3b\n"

      // If the instruction is INT 0, then this was probably the result
      // of playground::Library being unable to find a way to safely
      // rewrite the system call instruction. Retrieve the CPU register
      // at the time of the segmentation fault and invoke syscallWrapper().
    "8:cmpw $0xCD, (%%r15)\n"      // INT $0x0
      "jnz  9f\n"
#ifndef NDEBUG
      "lea  200f(%%rip), %%rdi\n"
      "call playground$debugMessage\n"
#endif
      "mov  0x98(%%rsp), %%rax\n"  // %rax at time of segmentation fault
      "mov  0x70(%%rsp), %%rdi\n"  // %rdi at time of segmentation fault
      "mov  0x78(%%rsp), %%rsi\n"  // %rsi at time of segmentation fault
      "mov  0x90(%%rsp), %%rdx\n"  // %rdx at time of segmentation fault
      "mov  0x40(%%rsp), %%r10\n"  // %r10 at time of segmentation fault
      "mov  0x30(%%rsp), %%r8\n"   // %r8  at time of segmentation fault
      "mov  0x38(%%rsp), %%r9\n"   // %r9  at time of segmentation fault
      "lea  7b(%%rip), %%rcx\n"
      "push %%rcx\n"
      "push 0xB8(%%rsp)\n"         // %rip at time of segmentation fault
      "lea  playground$syscallWrapper(%%rip), %%rcx\n"
      "jmp  *%%rcx\n"

      // This was a genuine segmentation fault. Trigger the kernel's default
      // signal disposition. The only way we can do this from seccomp mode
      // is by blocking the signal and retriggering it.
    "9:mov  $2, %%edi\n"           // stderr
      "lea  300f(%%rip), %%rsi\n"  // "Segmentation fault\n"
      "mov  $301f-300f, %%edx\n"
      "mov  $1, %%eax\n"           // NR_write
      "syscall\n"
      "orb  $4, 0x131(%%rsp)\n"    // signal mask at time of segmentation fault
      "ret\n"
#elif defined(__i386__)
      // Inspect instruction at the point where the segmentation fault
      // happened. If it is RDTSC, forward the request to the trusted
      // thread.
      "mov  $-3, %%ebx\n"          // request for RDTSC
      "mov  0x40(%%esp), %%ebp\n"  // %eip at time of segmentation fault
      "cmpw $0x310F, (%%ebp)\n"    // RDTSC
      "jz   0f\n"
      "cmpw $0x010F, (%%ebp)\n"
      "jnz  8f\n"
      "cmpb $0xF9, 2(%%ebp)\n"
      "jnz  8f\n"
      "mov  $-4, %%ebx\n"          // request for RDTSCP
    "0:"
#ifndef NDEBUG
      "lea  100f, %%eax\n"
      "push %%eax\n"
      "call playground$debugMessage\n"
      "sub  $4, %%esp\n"
#else
      "sub  $8, %%esp\n"
#endif
      "push %%ebx\n"
      "mov  %%fs:16, %%ebx\n"      // fd  = threadFdPub
      "mov  %%esp, %%ecx\n"        // buf = %esp
      "mov  $4, %%edx\n"           // len = sizeof(int)
    "1:mov  %%edx, %%eax\n"        // NR_write
      "int  $0x80\n"
      "cmp  %%eax, %%edx\n"
      "jz   5f\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   1b\n"
    "2:add  $12, %%esp\n"
      "movl $0, 0x34(%%esp)\n"     // %eax at time of segmentation fault
      "movl $0, 0x2C(%%esp)\n"     // %edx at time of segmentation fault
      "cmpw $0x310F, (%%ebp)\n"    // RDTSC
      "jz   3f\n"
      "movl $0, 0x30(%%esp)\n"     // %ecx at time of segmentation fault
    "3:addl $2, 0x40(%%esp)\n"     // %eip at time of segmentation fault
      "mov  0x40(%%esp), %%ebp\n"  // %eip at time of segmentation fault
      "cmpw $0x010F, (%%ebp)\n"    // RDTSC
      "jnz  4f\n"
      "addl $1, 0x40(%%esp)\n"     // %eip at time of segmentation fault
    "4:ret\n"
    "5:mov  $12, %%edx\n"          // len = 3*sizeof(int)
    "6:mov  $3, %%eax\n"           // NR_read
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   6b\n"
      "cmp  %%eax, %%edx\n"
      "jnz  2b\n"
      "pop  %%eax\n"
      "pop  %%edx\n"
      "pop  %%ecx\n"
      "mov  %%edx, 0x2C(%%esp)\n"  // %edx at time of segmentation fault
      "cmpw $0x310F, (%%ebp)\n"    // RDTSC
      "jz   7f\n"
      "mov  %%ecx, 0x30(%%esp)\n"  // %ecx at time of segmentation fault
    "7:mov  %%eax, 0x34(%%esp)\n"  // %eax at time of segmentation fault
      "jmp  3b\n"

      // If the instruction is INT 0, then this was probably the result
      // of playground::Library being unable to find a way to safely
      // rewrite the system call instruction. Retrieve the CPU register
      // at the time of the segmentation fault and invoke syscallWrapper().
    "8:cmpw $0xCD, (%%ebp)\n"      // INT $0x0
      "jnz  9f\n"
#ifndef NDEBUG
      "lea  200f, %%eax\n"
      "push %%eax\n"
      "call playground$debugMessage\n"
      "add  $0x4, %%esp\n"
#endif
      "mov  0x34(%%esp), %%eax\n"  // %eax at time of segmentation fault
      "mov  0x28(%%esp), %%ebx\n"  // %ebx at time of segmentation fault
      "mov  0x30(%%esp), %%ecx\n"  // %ecx at time of segmentation fault
      "mov  0x2C(%%esp), %%edx\n"  // %edx at time of segmentation fault
      "mov  0x1C(%%esp), %%esi\n"  // %esi at time of segmentation fault
      "mov  0x18(%%esp), %%edi\n"  // %edi at time of segmentation fault
      "mov  0x20(%%esp), %%ebp\n"  // %ebp at time of segmentation fault
      "call playground$syscallWrapper\n"
      "jmp  7b\n"

      // This was a genuine segmentation fault. Trigger the kernel's default
      // signal disposition. The only way we can do this from seccomp mode
      // is by blocking the signal and retriggering it.
    "9:mov  $2, %%ebx\n"           // stderr
      "lea  300f, %%ecx\n"         // "Segmentation fault\n"
      "mov  $301f-300f, %%edx\n"
      "mov  $4, %%eax\n"           // NR_write
      "int  $0x80\n"
      "orb  $4, 0x59(%%esp)\n"     // signal mask at time of segmentation fault
      "ret\n"
#else
#error Unsupported target platform
#endif
      ".pushsection \".rodata\"\n"
#ifndef NDEBUG
  "100:.asciz \"RDTSC(P): Executing handler\\n\"\n"
  "200:.asciz \"INT $0x0: Executing handler\\n\"\n"
#endif
  "300:.ascii \"Segmentation fault\\n\"\n"
  "301:\n"
      ".popsection\n"
  "999:pop  %0\n"
      : "=g"(fnc)
      :
      : "memory"
#if defined(__x86_64__)
        , "rsp"
#elif defined(__i386__)
        , "esp"
#endif
  );
  return fnc;
}

void Sandbox::snapshotMemoryMappings(int processFd) {
  SysCalls sys;
  int mapsFd = sys.open("/proc/self/maps", O_RDONLY, 0);
  if (mapsFd < 0 || !sendFd(processFd, mapsFd, -1, NULL, 0)) {
 failure:
    die("Cannot access /proc/self/maps");
  }
  NOINTR_SYS(sys.close(mapsFd));
  int dummy;
  if (read(sys, processFd, &dummy, sizeof(dummy)) != sizeof(dummy)) {
    goto failure;
  }
}

void Sandbox::startSandbox() {
  SysCalls sys;

  // The pid is unchanged for the entire program, so we can retrieve it once
  // and store it in a global variable.
  pid_                           = sys.getpid();

  // Block all signals, except for the RDTSC handler
  setupSignalHandlers();

  // Get socketpairs for talking to the trusted process
  int pair[4];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) ||
      socketpair(AF_UNIX, SOCK_STREAM, 0, pair+2)) {
    die("Failed to create trusted thread");
  }
  processFdPub_                  = pair[0];
  cloneFdPub_                    = pair[2];
  SecureMemArgs::Args* secureMem = createTrustedProcess(pair[0], pair[1],
                                                        pair[2], pair[3]);

  // We find all libraries that have system calls and redirect the system
  // calls to the sandbox. If we miss any system calls, the application will be
  // terminated by the kernel's seccomp code. So, from a security point of
  // view, if this code fails to identify system calls, we are still behaving
  // correctly.
  {
    Maps maps("/proc/self/maps");
    const char *libs[] = { "ld", "libc", "librt", "libpthread", NULL };

    // Intercept system calls in the VDSO segment (if any). This has to happen
    // before intercepting system calls in any of the other libraries, as
    // the main kernel entry point might be inside of the VDSO and we need to
    // determine its address before we can compare it to jumps from inside
    // other libraries.
    for (Maps::const_iterator iter = maps.begin(); iter != maps.end(); ++iter){
      Library* library = *iter;
      if (library->isVDSO() && library->parseElf()) {
        library->makeWritable(true);
        library->patchSystemCalls();
        library->makeWritable(false);
        break;
      }
    }

    // Intercept system calls in libraries that are known to have them.
    for (Maps::const_iterator iter = maps.begin(); iter != maps.end(); ++iter){
      Library* library = *iter;
      for (const char **ptr = libs; *ptr; ptr++) {
        const char *name = strstr(iter.name().c_str(), *ptr);
        if (name) {
          char ch = name[strlen(*ptr)];
          if (ch < 'A' || (ch > 'Z' && ch < 'a') || ch > 'z') {
            if (library->parseElf()) {
              library->makeWritable(true);
              library->patchSystemCalls();
              library->makeWritable(false);
              break;
            }
          }
        }
      }
    }
  }

  // Take a snapshot of the current memory mappings. These mappings will be
  // off-limits to all future mmap(), munmap(), mremap(), and mprotect() calls.
  snapshotMemoryMappings(processFdPub_);

  // Creating the trusted thread enables sandboxing
  createTrustedThread(processFdPub_, cloneFdPub_, secureMem);
}

} // namespace
