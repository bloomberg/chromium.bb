/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <fcntl.h>
#include <linux/net.h>
#include <sched.h>
#include <stdio.h>
#include <syscall.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <string.h>

#include "native_client/src/trusted/sandbox/linux/nacl_registers.h"
#include "native_client/src/trusted/sandbox/linux/nacl_syscall_checker.h"
#include "native_client/src/trusted/sandbox/linux/nacl_syscall_def.h"

#include "native_client/src/trusted/sandbox/linux/nacl_syscall_filter.h"

/*
 * TODO(bsy): fix this when we no longer need to build on
 * dapper. including <linux/sched.h> works fine on hardy and intrepid
 * (untested for jaunty), but breaks the build on dapper.  sigh.
 */
#if !defined(CLONE_STOPPED)
/* value copied from linux/sched.h for interop */
# define CLONE_STOPPED 0x02000000	/* Start in stopped state */
#endif

#define MAX_PATH 1024
#define NACL_SANDBOX_DEBUG 1

// int NaclSyscallFilter::kMaxPathLength = 1024;

// TODO:  Remove this class.
class DebugAllowOpenChecker : public SyscallChecker {
  bool RunRule(struct user_regs_struct* regs,
               struct SandboxState* state) {
    return true;
  }
};


class OpenChecker : public SyscallChecker {
 public:
  OpenChecker()
      : nacl_module_name_(""),
        nacl_module_name_set_(false) {
    SyscallChecker();
  }

  void set_nacl_module_name(const char* nacl_module_name) {
    nacl_module_name_ = nacl_module_name;
    nacl_module_name_set_ = true;
  }

  bool RunRule(struct user_regs_struct* regs,
               struct SandboxState* state) {
    if (!nacl_module_name_set_) {
      fprintf(stderr, "OpenChecker: RunRule() called before setup complete.\n");
      return false;
    }
    char flarray[NaclSyscallFilter::kMaxPathLength];
    char* fl = flarray;
    fl[0] = '\0';
    if (!Registers::GetStringVal(Registers::GetReg1(*regs),
                                 NaclSyscallFilter::kMaxPathLength,
                                 state->pid, fl)) {
      fprintf(stderr, "OpenChecker: Couldn't retrieve pathname\n");
      return false;
    }
#if NACL_SANDBOX_DEBUG > 1
    printf("Open file: %s  \n", fl);
#endif
    if (state->opened_untrusted) {
      fprintf(stderr, "OpenChecker: nacl module already opened.\n");
      return false;
    }
    if (strcmp(fl, nacl_module_name_) == 0) {
      state->opened_untrusted = true;
    }
    return true;
  }

 private:
  const char* nacl_module_name_;
  bool nacl_module_name_set_;
};

class SocketCallChecker : public SyscallChecker {
 public:
  bool RunRule(struct user_regs_struct* regs,
               struct SandboxState* state) {
    return true;
  }
};


class CloneChecker : public SyscallChecker {
 public:
  bool RunRule(struct user_regs_struct* regs,
               struct SandboxState* state) {
    int flags = Registers::GetReg1(*regs) | CLONE_PTRACE;
    if ((flags & (CLONE_THREAD)) == 0) {
      printf("CLONE_THREAD[%d]: Clone called with flags: %08x\n",
             state->pid, flags);
      return false;
    }
    if ((flags & (CLONE_VFORK)) != 0) {
      printf("CLONE_VFORK[%d]: Clone called with flags: %08x\n",
             state->pid, flags);
      return false;
    }
    if ((flags & (CLONE_STOPPED)) != 0) {
      printf("CLONE_STOPPED[%d]: Clone called with flags: %08x\n",
             state->pid, flags);
      return false;
    }
    if ((flags & (CLONE_NEWNS)) != 0) {
      printf("CLONE_NEWNS[%d]: Clone called with flags: %08x\n",
             state->pid, flags);
      return false;
    }
    /* Unfortunately libc sets CLONE_THREAD. Remove all just in case */
    flags = flags & ~(CLONE_UNTRACED|CLONE_STOPPED|CLONE_VFORK|CLONE_NEWNS);
    Registers::SetReg2(regs, flags);
    return true;
  }
};


class FcntlChecker : public SyscallChecker {
 public:
  bool RunRule(struct user_regs_struct* regs,
               struct SandboxState* state) {
    int cmd = Registers::GetReg1(*regs);
    if (cmd == F_SETFD) {
      printf("F_SETFD[%d]: Fcntl called with cmd: %08x which is not valid\n",
             state->pid, cmd);
      return false;
    }
    // TODO: This triggers in naclquake_debug.  Change to false when
    // possible.
    if (cmd == F_SETSIG) {
      printf("F_SETSIG[%d]: Fcntl called with cmd: %08x which is not valid\n",
             state->pid, cmd);
      return true;
    }
    if (cmd == F_SETFL) {
      // Check flags
      int args = Registers::GetReg2(*regs);
      if ((args & O_ASYNC) != 0 || (args & O_DIRECT) != 0) {
        printf("F_SETFL[%d]: Fcntl called with args: %08x which is not valid\n",
             state->pid, args);
        return false;
      }
    }
    return true;
  }
};


class KillChecker : public SyscallChecker {
 public:
  bool RunRule(struct user_regs_struct* regs,
               struct SandboxState* state) {
    int sig = Registers::GetReg2(*regs);
    if (sig == 0) {
      return true;
    }
    return false;
  }
};


NaclSyscallFilter::NaclSyscallFilter(const char* nacl_module_name)
    : nacl_module_name_(nacl_module_name) {
  state_.opened_untrusted = false;
  state_.done_exec = false;
  InitSyscallRules();
}


NaclSyscallFilter::~NaclSyscallFilter() {
  for (int i = 0; i < NACL_SYSCALL_TABLE_SIZE; ++i) {
    delete syscall_rules_[i];
  }
}


void NaclSyscallFilter::InitSyscallRules() {
  for (int i = 0; i < NACL_SYSCALL_TABLE_SIZE; ++i) {
    syscall_rules_[i] = new SyscallChecker();
    syscall_calls_[i] = 0;
    syscall_rules_[i]->set_once(false);
  }
  // Set allowed things here.  Initially on SyscallChecker
  // construction, a syscall is not allowed, but RunRule will pass.

  syscall_rules_[__NR_execve]->set_allowed(true);
  syscall_rules_[__NR_execve]->set_once(true);

  delete syscall_rules_[__NR_kill];
  syscall_rules_[__NR_kill] = NULL;
  syscall_rules_[__NR_kill] = new KillChecker();
  syscall_rules_[__NR_kill]->set_allowed(true);

  // TODO: Proxy open calls.
  delete syscall_rules_[__NR_open];
  syscall_rules_[__NR_open] = NULL;
  DebugAllowOpenChecker* chk = new DebugAllowOpenChecker();
  syscall_rules_[__NR_open] = chk;
  syscall_rules_[__NR_open]->set_allowed(true);

  delete syscall_rules_[__NR_clone];
  syscall_rules_[__NR_clone] = NULL;
  syscall_rules_[__NR_clone] = new CloneChecker();
  syscall_rules_[__NR_clone]->set_allowed(true);

#ifdef __NR_socketcall
  // TODO: Proxy certain socketcalls.
  delete syscall_rules_[__NR_socketcall];
  syscall_rules_[__NR_socketcall] = NULL;
  syscall_rules_[__NR_socketcall] = new SocketCallChecker();
  syscall_rules_[__NR_socketcall]->set_allowed(true);
#endif

#ifdef __NR_fcntl
  delete syscall_rules_[__NR_fcntl];
  syscall_rules_[__NR_fcntl] = NULL;
  syscall_rules_[__NR_fcntl] = new FcntlChecker();
  syscall_rules_[__NR_fcntl]->set_allowed(true);
#endif

#ifdef __NR_fcntl64
  delete syscall_rules_[__NR_fcntl64];
  syscall_rules_[__NR_fcntl64] = NULL;
  syscall_rules_[__NR_fcntl64] = new FcntlChecker();
  syscall_rules_[__NR_fcntl64]->set_allowed(true);
#endif


#ifdef __NR_exit
  syscall_rules_[__NR_exit]->set_allowed(true);
#endif

#ifdef __NR_time
  syscall_rules_[__NR_time]->set_allowed(true);
#endif

#ifdef __NR_wait4
  syscall_rules_[__NR_wait4]->set_allowed(true);
#endif

#ifdef __NR_getpid
  syscall_rules_[__NR_getpid]->set_allowed(true);
#endif

#ifdef __NR_getuid
  syscall_rules_[__NR_getuid]->set_allowed(true);
#endif

#ifdef __NR_ioctl
  syscall_rules_[__NR_ioctl]->set_allowed(true);
#endif

#ifdef __NR_brk
  syscall_rules_[__NR_brk]->set_allowed(true);
#endif

#ifdef __NR_munmap
  syscall_rules_[__NR_munmap]->set_allowed(true);
#endif

#ifdef __NR_mprotect
  syscall_rules_[__NR_mprotect]->set_allowed(true);
#endif

#ifdef __NR_mlock
  syscall_rules_[__NR_mlock]->set_allowed(true);
#endif

#ifdef __NR_madvise
  syscall_rules_[__NR_madvise]->set_allowed(true);
#endif

#ifdef __NR_gettimeofday
  syscall_rules_[__NR_gettimeofday]->set_allowed(true);
#endif

#ifdef __NR_pread64
  syscall_rules_[__NR_pread64]->set_allowed(true);
#endif

#ifdef __NR_pwrite64
  syscall_rules_[__NR_pwrite64]->set_allowed(true);
#endif

#ifdef __NR_ugetrlimit
  syscall_rules_[__NR_ugetrlimit]->set_allowed(true);
#endif

#ifdef __NR_getrlimit
  syscall_rules_[__NR_getrlimit]->set_allowed(true);
#endif

#ifdef __NR_setrlimit
  syscall_rules_[__NR_setrlimit]->set_allowed(true);
#endif

#ifdef __NR_poll
  syscall_rules_[__NR_poll]->set_allowed(true);
#endif

#ifdef __NR_clock_gettime
  syscall_rules_[__NR_clock_gettime]->set_allowed(true);
#endif

#ifdef __NR_clock_getres
  syscall_rules_[__NR_clock_getres]->set_allowed(true);
#endif

#ifdef __NR_nanosleep
  syscall_rules_[__NR_nanosleep]->set_allowed(true);
#endif

  // TODO: remove ipc() if possible
#ifdef __NR_ipc
  syscall_rules_[__NR_ipc]->set_allowed(true);
#endif

#ifdef __NR__newselect
  syscall_rules_[__NR__newselect]->set_allowed(true);
#endif

#ifdef __NR_read
  syscall_rules_[__NR_read]->set_allowed(true);
#endif

#ifdef __NR_write
  syscall_rules_[__NR_write]->set_allowed(true);
#endif

#ifdef __NR_readv
  syscall_rules_[__NR_readv]->set_allowed(true);
#endif

#ifdef __NR_writev
  syscall_rules_[__NR_writev]->set_allowed(true);
#endif

#ifdef __NR_close
  syscall_rules_[__NR_close]->set_allowed(true);
#endif

#ifdef __NR_getdents64
  syscall_rules_[__NR_getdents64]->set_allowed(true);
#endif

#ifdef __NR_getdents
  syscall_rules_[__NR_getdents]->set_allowed(true);
#endif

#ifdef __NR_mmap
  syscall_rules_[__NR_mmap]->set_allowed(true);
#endif

#ifdef __NR_mmap2
  syscall_rules_[__NR_mmap2]->set_allowed(true);
#endif

#ifdef __NR_access
  syscall_rules_[__NR_access]->set_allowed(true);
#endif


#ifdef __NR_sigaltstack
  syscall_rules_[__NR_sigaltstack]->set_allowed(true);
#endif

#ifdef __NR_modify_ldt
  syscall_rules_[__NR_modify_ldt]->set_allowed(true);
#endif

#ifdef __NR_dup
  syscall_rules_[__NR_dup]->set_allowed(true);
#endif

#ifdef __NR_uname
  syscall_rules_[__NR_uname]->set_allowed(true);
#endif

#ifdef __NR_stat64
  syscall_rules_[__NR_stat64]->set_allowed(true);
#endif

#ifdef __NR_fstat64
  syscall_rules_[__NR_fstat64]->set_allowed(true);
#endif

#ifdef __NR_lstat64
  syscall_rules_[__NR_lstat64]->set_allowed(true);
#endif

#ifdef __NR_set_thread_area
  syscall_rules_[__NR_set_thread_area]->set_allowed(true);
#endif

#ifdef __NR_set_tid_address
  syscall_rules_[__NR_set_tid_address]->set_allowed(true);
#endif

#ifdef __NR_rt_sigaction
  syscall_rules_[__NR_rt_sigaction]->set_allowed(true);
#endif

#ifdef __NR_exit_group
  syscall_rules_[__NR_exit_group]->set_allowed(true);
#endif

#ifdef __NR_rt_sigprocmask
  syscall_rules_[__NR_rt_sigprocmask]->set_allowed(true);
#endif

#ifdef __NR__sysctl
  syscall_rules_[__NR__sysctl]->set_allowed(true);
#endif

#ifdef __NR_futex
  syscall_rules_[__NR_futex]->set_allowed(true);
#endif

#ifdef __NR__llseek
  syscall_rules_[__NR__llseek]->set_allowed(true);
#endif

#ifdef __NR_lseek
  syscall_rules_[__NR_lseek]->set_allowed(true);
#endif

#ifdef __NR_pipe
  syscall_rules_[__NR_pipe]->set_allowed(true);
#endif

#ifdef __NR_sched_getparam
  syscall_rules_[__NR_sched_getparam]->set_allowed(true);
#endif

#ifdef __NR_set_robust_list
  syscall_rules_[__NR_set_robust_list]->set_allowed(true);
#endif

#ifdef __NR_sched_getscheduler
  syscall_rules_[__NR_sched_getscheduler]->set_allowed(true);
#endif

#ifdef __NR_sched_get_priority_min
  syscall_rules_[__NR_sched_get_priority_min]->set_allowed(true);
#endif

#ifdef __NR_sched_get_priority_max
  syscall_rules_[__NR_sched_get_priority_max]->set_allowed(true);
#endif

#ifdef __NR_readlink
  syscall_rules_[__NR_readlink]->set_allowed(true);
#endif

#ifdef __NR_geteuid32
  syscall_rules_[__NR_geteuid32]->set_allowed(true);
#endif

#ifdef __NR_getegid32
  syscall_rules_[__NR_getegid32]->set_allowed(true);
#endif

#ifdef __NR_getgid32
  syscall_rules_[__NR_getgid32]->set_allowed(true);
#endif

#ifdef __NR_getuid32
  syscall_rules_[__NR_getuid32]->set_allowed(true);
#endif

  // TODO: remove when the SDL situation is sorted out.
#ifdef __NR_unlink
  syscall_rules_[__NR_unlink]->set_allowed(true);
#endif

#ifdef __NR_chdir
  syscall_rules_[__NR_chdir]->set_allowed(true);
#endif

#ifdef __NR_sched_yield
  syscall_rules_[__NR_sched_yield]->set_allowed(true);
#endif

#ifdef __NR_ftruncate
  syscall_rules_[__NR_ftruncate]->set_allowed(true);
#endif

}


void NaclSyscallFilter::PrintAllowedRules() {
  printf("Allowed syscalls: \n");
  for (int i = 0; i < NACL_SYSCALL_TABLE_SIZE; ++i) {
    if (syscall_rules_[i]->allowed()) {
      printf("  %s\t", GetSyscallName(i));
    }
  }
}


void NaclSyscallFilter::DebugPrintBadSyscalls() {
  printf("Bad syscalls: \n");
  for (int i = 0; i < NACL_SYSCALL_TABLE_SIZE; ++i) {
    if (syscall_calls_[i] > 0) {
      printf("  %s [%d]\n", GetSyscallName(i), syscall_calls_[i]);
    }
  }
}


bool NaclSyscallFilter::RunRule(pid_t pid, struct user_regs_struct* regs,
                                bool entering) {
  int syscall_num = (int)Registers::GetSyscall(*regs);
#if NACL_SANDBOX_DEBUG > 1
  if (!entering) {
    printf("[%d] x: %d \n", pid, syscall_num);
    return true;
  } else {
    printf("[%d] n: %d \n", pid, syscall_num);
  }
#endif
  if (!state_.done_exec) {
    if (syscall_num == __NR_execve) {
      state_.done_exec = true;
    }
  }
  state_.pid = pid;
  if (0 > syscall_num || syscall_num >= NACL_SYSCALL_TABLE_SIZE) {
    printf("Syscall out of range: %d [%s]\n",
           syscall_num, GetSyscallName(syscall_num));
    return false;
    // Kill sandbox
  }
  SyscallChecker* checker = syscall_rules_[syscall_num];
  if (!checker->allowed()) {
    printf("Syscall not allowed: %d [%s]\n",
           syscall_num, GetSyscallName(syscall_num));
    syscall_calls_[syscall_num] = syscall_calls_[syscall_num] + 1;
    return false;
    // Kill sandbox
  }
  if (checker->once()) {
    checker->set_allowed(false);
  }
  if (!checker->RunRule(regs, &state_)) {
    printf("Syscall rule execution failed: %d [%s]\n",
           syscall_num, GetSyscallName(syscall_num));
    syscall_calls_[syscall_num] = syscall_calls_[syscall_num] + 1;
    return false;
    // Kill sandbox
  }
  return true;
}
