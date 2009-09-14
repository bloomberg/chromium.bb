#ifndef SANDBOX_IMPL_H__
#define SANDBOX_IMPL_H__

#include <asm/ldt.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/prctl.h>
#include <linux/unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define NOINTR_SYS(x)                                                         \
  ({ typeof(x) i__; while ((i__ = (x)) < 0 && sys.my_errno == EINTR); i__;})

#ifdef __cplusplus
#include <iostream>
#include <map>
#include <vector>
#include "sandbox.h"
#include "securemem.h"
#include "tls.h"

namespace playground {

class Sandbox {
  // TODO(markus): restrict access to our private file handles
 public:
  enum { kMaxThreads = 100 };

  // This is the main public entry point. It finds all system calls that
  // need rewriting, sets up the resources needed by the sandbox, and
  // enters Seccomp mode.
  static void startSandbox() asm("StartSeccompSandbox");

 private:
// syscall_table.c has to be implemented in C, as C++ does not support
// designated initializers for arrays. The only other alternative would be
// to have a source code generator for this table.
//
// We would still like the C source file to include our header file. This
// requires some define statements to transform C++ specific constructs to
// something that is palatable to a C compiler.
#define STATIC static
#define SecureMemArgs SecureMem::Args
  // Clone() is special as it has a wrapper in syscall_table.c. The wrapper
  // adds one extra argument (the pointer to the saved registers) and then
  // calls playground$sandbox__clone().
  static int sandbox_clone(int flags, void* stack, int* pid, int* ctid,
                           void* tls, void* wrapper_sp)
                                            asm("playground$sandbox__clone");
#else
#define STATIC
#define bool int
#define SecureMemArgs void
  // This is the wrapper entry point that is found in the syscall_table.
  int sandbox_clone(int flags, void* stack, int* pid, int* ctid, void* tls)
                                            asm("playground$sandbox_clone");
#endif

  // Entry points for sandboxed code that is attempting to make system calls
  STATIC int sandbox_access(const char*, int)
                                          asm("playground$sandbox_access");
  STATIC int sandbox_exit(int status)     asm("playground$sandbox_exit");
  STATIC int sandbox_getpid()             asm("playground$sandbox_getpid");
  #if defined(__NR_getsockopt)
  STATIC int sandbox_getsockopt(int, int, int, void*, socklen_t*)
                                          asm("playground$sandbox_getsockopt");
  #endif
  STATIC int sandbox_gettid()             asm("playground$sandbox_gettid");
  STATIC int sandbox_ioctl(int d, int req, void* arg)
                                          asm("playground$sandbox_ioctl");
  #if defined(__NR_ipc)
  STATIC int sandbox_ipc(unsigned, int, int, int, void*, long)
                                          asm("playground$sandbox_ipc");
  #endif
  STATIC int sandbox_madvise(void*, size_t, int)
                                          asm("playground$sandbox_madvise");
  STATIC void *sandbox_mmap(void* start, size_t length, int prot, int flags,
                            int fd, off_t offset)
                                          asm("playground$sandbox_mmap");
  STATIC int sandbox_mprotect(const void*, size_t, int)
                                          asm("playground$sandbox_mprotect");
  STATIC int sandbox_munmap(void* start, size_t length)
                                          asm("playground$sandbox_munmap");
  STATIC int sandbox_open(const char*, int, mode_t)
                                          asm("playground$sandbox_open");
  #if defined(__NR_recvfrom)
  STATIC ssize_t sandbox_recvfrom(int, void*, size_t, int, void*, socklen_t*)
                                          asm("playground$sandbox_recvfrom");
  STATIC ssize_t sandbox_recvmsg(int, struct msghdr*, int)
                                          asm("playground$sandbox_recvmsg");
  STATIC size_t sandbox_sendmsg(int, const struct msghdr*, int)
                                          asm("playground$sandbox_sendmsg");
  STATIC ssize_t sandbox_sendto(int, const void*, size_t, int, const void*,
                                socklen_t)asm("playground$sandbox_sendto");
  #if defined(__NR_shmat)
  STATIC void* sandbox_shmat(int, const void*, int)
                                          asm("playground$sandbox_shmat");
  STATIC int sandbox_shmctl(int, int, void*)
                                          asm("playground$sandbox_shmctl");
  STATIC int sandbox_shmdt(const void*)   asm("playground$sandbox_shmdt");
  STATIC int sandbox_shmget(int, size_t, int)
                                          asm("playground$sandbox_shmget");
  #endif
  STATIC int sandbox_setsockopt(int, int, int, const void*, socklen_t)
                                          asm("playground$sandbox_setsockopt");
  #endif
  #if defined(__NR_socketcall)
  STATIC int sandbox_socketcall(int call, void* args)
                                          asm("playground$sandbox_socketcall");
  #endif
  STATIC int sandbox_stat(const char* path, void* buf)
                                          asm("playground$sandbox_stat");
  #if defined(__NR_stat64)
  STATIC int sandbox_stat64(const char *path, void* b)
                                          asm("playground$sandbox_stat64");
  #endif

  // Functions for system calls that need to be handled in the trusted process
  STATIC bool process_access(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_access");
  STATIC bool process_clone(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_clone");
  STATIC bool process_exit(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_exit");
  #if defined(__NR_getsockopt)
  STATIC bool process_getsockopt(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_getsockopt");
  #endif
  STATIC bool process_ioctl(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_ioctl");
  #if defined(__NR_ipc)
  STATIC bool process_ipc(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_ipc");
  #endif
  STATIC bool process_madvise(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_madvise");
  STATIC bool process_mmap(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_mmap");
  STATIC bool process_mprotect(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_mprotect");
  STATIC bool process_munmap(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_munmap");
  STATIC bool process_open(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_open");
  #if defined(__NR_recvfrom)
  STATIC bool process_recvfrom(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_recvfrom");
  STATIC bool process_recvmsg(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_recvmsg");
  STATIC bool process_sendmsg(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_sendmsg");
  STATIC bool process_sendto(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_sendto");
  STATIC bool process_setsockopt(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_setsockopt");
  #endif
  #if defined(__NR_shmat)
  STATIC bool process_shmat(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_shmat");
  STATIC bool process_shmctl(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_shmctl");
  STATIC bool process_shmdt(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_shmdt");
  STATIC bool process_shmget(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_shmget");
  #endif
  #if defined(__NR_socketcall)
  STATIC bool process_socketcall(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_socketcall");
  #endif
  STATIC bool process_stat(int, int, int, int, SecureMemArgs*)
                                          asm("playground$process_stat");

#ifdef __cplusplus
  friend class Debug;
  friend class Library;
  friend class Maps;
  friend class Mutex;
  friend class SecureMem;
  friend class TLS;

  // Define our own inline system calls. These calls will not be rewritten
  // to point to the sandboxed wrapper functions. They thus allow us to
  // make actual system calls (e.g. in the sandbox initialization code, and
  // in the trusted process)
  class SysCalls {
   public:
    #define SYS_CPLUSPLUS
    #define SYS_ERRNO     my_errno
    #define SYS_INLINE    inline
    #define SYS_PREFIX    -1
    #undef  SYS_LINUX_SYSCALL_SUPPORT_H
    #include "linux_syscall_support.h"
    SysCalls() : my_errno(0) { }
    int my_errno;
  };
  #ifdef __NR_mmap2
    #define      MMAP      mmap2
    #define __NR_MMAP __NR_mmap2
  #else
    #define      MMAP      mmap
    #define __NR_MMAP __NR_mmap
  #endif

  // Print an error message and terminate the program. Used for fatal errors.
  static void die(const char *msg = 0) __attribute__((noreturn)) {
    SysCalls sys;
    if (msg) {
      sys.write(2, msg, strlen(msg));
      sys.write(2, "\n", 1);
    }
    for (;;) {
      sys.exit_group(1);
      sys._exit(1);
    }
  }

  // Wrapper around "read()" that can deal with partial and interrupted reads
  // and that does not modify the global errno variable.
  static ssize_t read(SysCalls& sys, int fd, void* buf, size_t len) {
    if (len < 0) {
      sys.my_errno = EINVAL;
      return -1;
    }
    size_t offset = 0;
    while (offset < len) {
      ssize_t partial =
          NOINTR_SYS(sys.read(fd, reinterpret_cast<char*>(buf) + offset,
                              len - offset));
      if (partial < 0) {
        return partial;
      } else if (!partial) {
        break;
      }
      offset += partial;
    }
    return offset;
  }

  // Wrapper around "write()" that can deal with interrupted writes and that
  // does not modify the global errno variable.
  static ssize_t write(SysCalls& sys, int fd, const void* buf, size_t len){
    return NOINTR_SYS(sys.write(fd, buf, len));
  }

  // Sends a file handle to another process.
  static bool sendFd(int transport, int fd0, int fd1, const void* buf,
                     size_t len) asm("playground$sendFd");

  // If getFd() fails, it will set the first valid fd slot (e.g. fd0) to
  // -errno.
  static bool getFd(int transport, int* fd0, int* fd1, void* buf,
                    size_t* len);

  // Data structures used to forward system calls to the trusted process.
  struct Accept {
    int        sockfd;
    void*      addr;
    socklen_t* addrlen;
  } __attribute__((packed));

  struct Accept4 {
    int        sockfd;
    void*      addr;
    socklen_t* addrlen;
    int        flags;
  } __attribute__((packed));

  struct Access {
    size_t path_length;
    int    mode;
  } __attribute__((packed));

  struct Bind {
    int       sockfd;
    void*     addr;
    socklen_t addrlen;
  } __attribute__((packed));

  struct Clone {
    int       flags;
    void*     stack;
    int*      pid;
    int*      ctid;
    void*     tls;
    #if defined(__x86_64__)
      struct {
        void* r15;
        void* r14;
        void* r13;
        void* r12;
        void* r11;
        void* r10;
        void* r9;
        void* r8;
        void* rdi;
        void* rsi;
        void* rdx;
        void* rcx;
        void* rbx;
        void* rbp;
        void* fake_ret;
      } regs64 __attribute__((packed));
    #elif defined(__i386__)
      struct {
        void* ebp;
        void* edi;
        void* esi;
        void* edx;
        void* ecx;
        void* ebx;
        void* ret2;
      } regs32 __attribute__((packed));
    #else
    #error Unsupported target platform
    #endif
    void*     ret;
  } __attribute__((packed));

  struct Connect {
    int       sockfd;
    void*     addr;
    socklen_t addrlen;
  } __attribute__((packed));

  struct GetSockName {
    int        sockfd;
    void*      name;
    socklen_t* namelen;
  } __attribute__((packed));

  struct GetPeerName {
    int        sockfd;
    void*      name;
    socklen_t* namelen;
  } __attribute__((packed));

  struct GetSockOpt {
    int        sockfd;
    int        level;
    int        optname;
    void*      optval;
    socklen_t* optlen;
  } __attribute__((packed));

  struct IOCtl {
    int  d;
    int  req;
    void *arg;
  } __attribute__((packed));

  #if defined(__NR_ipc)
  struct IPC {
    unsigned call;
    int      first;
    int      second;
    int      third;
    void*    ptr;
    long     fifth;
  } __attribute__((packed));
  #endif

  struct Listen {
    int sockfd;
    int backlog;
  } __attribute__((packed));

  struct MAdvise {
    const void*  start;
    size_t       len;
    int          advice;
  } __attribute__((packed));

  struct MMap {
    void*  start;
    size_t length;
    int    prot;
    int    flags;
    int    fd;
    off_t  offset;
  } __attribute__((packed));

  struct MProtect {
    const void*  addr;
    size_t       len;
    int          prot;
  };

  struct MUnmap {
    void*  start;
    size_t length;
  } __attribute__((packed));

  struct Open {
    size_t path_length;
    int    flags;
    mode_t mode;
  } __attribute__((packed));

  struct Recv {
    int    sockfd;
    void*  buf;
    size_t len;
    int    flags;
  } __attribute__((packed));

  struct RecvFrom {
    int       sockfd;
    void*     buf;
    size_t    len;
    int       flags;
    void*     from;
    socklen_t *fromlen;
  } __attribute__((packed));

  struct RecvMsg {
    int                  sockfd;
    struct msghdr*       msg;
    int                  flags;
  } __attribute__((packed));

  struct Send {
    int         sockfd;
    const void* buf;
    size_t      len;
    int         flags;
  } __attribute__((packed));

  struct SendMsg {
    int                  sockfd;
    const struct msghdr* msg;
    int                  flags;
  } __attribute__((packed));

  struct SendTo {
    int         sockfd;
    const void* buf;
    size_t      len;
    int         flags;
    const void* to;
    socklen_t   tolen;
  } __attribute__((packed));

  struct SetSockOpt {
    int         sockfd;
    int         level;
    int         optname;
    const void* optval;
    socklen_t   optlen;
  } __attribute__((packed));

  #if defined(__NR_shmat)
  struct ShmAt {
    int         shmid;
    const void* shmaddr;
    int         shmflg;
 } __attribute__((packed));

  struct ShmCtl {
    int  shmid;
    int  cmd;
    void *buf;
  } __attribute__((packed));

  struct ShmDt {
    const void *shmaddr;
  } __attribute__((packed));

  struct ShmGet {
    int    key;
    size_t size;
    int    shmflg;
  } __attribute__((packed));
  #endif

  struct ShutDown {
    int sockfd;
    int how;
  } __attribute__((packed));

  struct Socket {
    int domain;
    int type;
    int protocol;
  } __attribute__((packed));

  struct SocketPair {
    int  domain;
    int  type;
    int  protocol;
    int* pair;
  } __attribute__((packed));

  #if defined(__NR_socketcall)
  struct SocketCall {
    int    call;
    void*  arg_ptr;
    union {
      Socket      socket;
      Bind        bind;
      Connect     connect;
      Listen      listen;
      Accept      accept;
      GetSockName getsockname;
      GetPeerName getpeername;
      SocketPair  socketpair;
      Send        send;
      Recv        recv;
      SendTo      sendto;
      RecvFrom    recvfrom;
      ShutDown    shutdown;
      SetSockOpt  setsockopt;
      GetSockOpt  getsockopt;
      SendMsg     sendmsg;
      RecvMsg     recvmsg;
      Accept4     accept4;
    } args;
  } __attribute__((packed));
  #endif

  struct Stat {
    int    sysnum;
    size_t path_length;
    void*  buf;
  } __attribute__((packed));

  // Thread local data available from each sandboxed thread.
  enum { TLS_COOKIE, TLS_TID, TLS_THREAD_FD };
  static long long cookie() { return TLS::getTLSValue<long long>(TLS_COOKIE); }
  static int tid()          { return TLS::getTLSValue<int>(TLS_TID); }
  static int threadFdPub()  { return TLS::getTLSValue<int>(TLS_THREAD_FD); }
  static int processFdPub() { return processFdPub_; }

  // The SEGV handler knows how to handle RDTSC instructions
  static void setupSignalHandlers();
  static void (*segv())(int signo);

  // If no specific handler has been registered for a system call, call this
  // function which asks the trusted thread to perform the call. This is used
  // for system calls that are not restricted.
  static void* defaultSystemCallHandler(int syscallNum, void* arg0,
                                        void* arg1, void* arg2, void* arg3,
                                        void* arg4, void* arg5)
                                    asm("playground$defaultSystemCallHandler");

  // Return a secure memory structure that can be used by a newly created
  // thread.
  static SecureMem::Args* getSecureMem();

  // This functions runs in the trusted process at startup and finds all the
  // memory mappings that existed when the sandbox was first enabled. Going
  // forward, all these mappings are off-limits for operations such as
  // mmap(), munmap(), and mprotect().
  static void  initializeProtectedMap(int fd);

  // Helper functions that allows the trusted process to get access to
  // "/proc/self/maps" in the sandbox.
  static void  snapshotMemoryMappings(int processFd);

  // Main loop for the trusted process.
  static void  trustedProcess(int parentProc, int processFdPub, int sandboxFd,
                              int cloneFd, SecureMem::Args* secureArena)
                                                     __attribute__((noreturn));

  // Fork()s of the trusted process.
  static SecureMem::Args* createTrustedProcess(int processFdPub, int sandboxFd,
                                               int cloneFdPub, int cloneFd);

  // Creates the trusted thread for the initial thread, then enables
  // Seccomp mode.
  static void  createTrustedThread(int processFdPub, int cloneFdPub,
                                   SecureMem::Args* secureMem);

  static int   pid_;
  static int   processFdPub_;
  static int   cloneFdPub_;

  #ifdef __i386__
  struct SocketCallArgInfo;
  static const struct SocketCallArgInfo socketCallArgInfo[];
  #endif

  // The syscall_mutex_ can only be directly accessed by the trusted process.
  // It can be accessed by the trusted thread after fork()ing and calling
  // mprotect(PROT_READ|PROT_WRITE). The mutex is used for system calls that
  // require passing additional data, and that require the trusted process to
  // wait until the trusted thread is done processing (e.g. exit(), clone(),
  // open(), stat())
  static int syscall_mutex_ asm("playground$syscall_mutex");

  // Available in trusted process, only
  typedef std::map<void *, long>       ProtectedMap;
  static ProtectedMap                  protectedMap_;
  static std::vector<SecureMem::Args*> secureMemPool_;
};

} // namespace

using playground::Sandbox;
#endif // __cplusplus

#endif // SANDBOX_IMPL_H__
