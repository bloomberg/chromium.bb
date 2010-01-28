/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <sys/user.h>
#include <stdint.h>
#include <string.h>

#include "native_client/src/trusted/sandbox/linux/nacl_registers.h"
#include "native_client/src/trusted/sandbox/linux/nacl_syscall_checker.h"
#include "native_client/src/trusted/sandbox/linux/nacl_syscall_filter.h"
#include "native_client/src/trusted/sandbox/linux/nacl_sandbox.h"

/* 0x4200-0x4300 are reserved for architecture-independent additions.  */
#define PTRACE_SETOPTIONS       (enum __ptrace_request)0x4200
#define PTRACE_GETEVENTMSG      (enum __ptrace_request)0x4201
#define PTRACE_GETSIGINFO       (enum __ptrace_request)0x4202
#define PTRACE_SETSIGINFO       (enum __ptrace_request)0x4203

/* options set using PTRACE_SETOPTIONS */
#define PTRACE_O_TRACESYSGOOD   0x00000001
#define PTRACE_O_TRACEFORK      0x00000002
#define PTRACE_O_TRACEVFORK     0x00000004
#define PTRACE_O_TRACECLONE     0x00000008
#define PTRACE_O_TRACEEXEC      0x00000010
#define PTRACE_O_TRACEVFORKDONE 0x00000020
#define PTRACE_O_TRACEEXIT      0x00000040

#define PTRACE_EVENT_FORK       1
#define PTRACE_EVENT_VFORK      2
#define PTRACE_EVENT_CLONE      3
#define PTRACE_EVENT_EXEC       4
#define PTRACE_EVENT_VFORK_DONE 5
#define PTRACE_EVENT_EXIT       6

#define NACL_SANDBOX_DEBUG 1

const char* NaclSandbox::kSelLdrPathSuffix = ".trace";
const char* NaclSandbox::kTraceSandboxEnvVariable =
    "NACL_ENABLE_OUTER_SANDBOX";

ThreadState::ThreadState() {
  thread_state_ = new std::map<int, struct traced_child>;
}

ThreadState::~ThreadState() {
  if (thread_state_)
    delete thread_state_;
  thread_state_ = NULL;
}

bool ThreadState::ChangeThreadSyscallStatus(int tid,
                                            struct user_regs_struct* regs) {
  std::map<int, struct traced_child>::iterator it = thread_state_->find(tid);
  if (it != thread_state_->end()) {
    if (it->second.in_syscall == false) {
      it->second.in_syscall = true;
      it->second.current_syscall = regs->orig_eax;
    } else {
      it->second.in_syscall = false;
    }
    return true;
  }
  printf("Error!  Acting on uninitialized thread %d\n", tid);
  return false;
}

const bool ThreadState::ThreadEnteringSyscall(int tid,
                                              struct user_regs_struct* regs,
                                              bool* error) {
  bool entering;
  std::map<int, struct traced_child>::const_iterator it =
      thread_state_->find(tid);
  if (it != thread_state_->end()) {
    if (it->second.needs_setup) {
      printf("Error!  Acting on uninitialized thread %d\n", tid);
      *error = true;
      entering = false;
    } else {
      entering = !it->second.in_syscall;
    }
  } else {
    entering = false;
  }
  if (entering && regs->eax != -ENOSYS) {
    printf("[%d] MISMATCH between my thread state and ENOSYS, %d\n",
           tid, (int)regs->orig_eax);
    *error = true;
  }
  if (it->second.in_syscall &&
      (regs->orig_eax != it->second.current_syscall)) {
    printf("[%d] MISMATCH between entering and exiting syscall, %d\n",
           tid, (int)regs->orig_eax);
    *error = true;
  }
  return entering;
}

void ThreadState::AddThread(int tid) {
  std::map<int, struct traced_child>::iterator it = thread_state_->find(tid);
  if (it == thread_state_->end()) {
    struct traced_child new_child;
    new_child.in_syscall = false;
    new_child.needs_setup = true;
    new_child.current_syscall = -1;
    thread_state_->insert(std::pair<int, struct traced_child>(tid, new_child));
#if NACL_SANDBOX_DEBUG > 1
    printf("Adding thread %d\n", tid);
#endif
  } else {
    printf("Adding a thread twice: %d \n", tid);
  }
}

void ThreadState::RemoveThread(int tid) {
  std::map<int, struct traced_child>::iterator it = thread_state_->find(tid);
  if (it != thread_state_->end()) {
    thread_state_->erase(it);
#if NACL_SANDBOX_DEBUG > 1
    printf("Removing thread %d\n", tid);
#endif
  } else {
    printf("RemoveThread: Trying to remove nonexistent thread %d\n", tid);
  }
}

const bool ThreadState::ThreadNeedsSetup(int tid) {
  std::map<int, struct traced_child>::iterator it = thread_state_->find(tid);
  if (it != thread_state_->end()) {
    return it->second.needs_setup;
  }
  printf("ThreadNeedsSetup: Trying to setup nonexistent thread %d\n", tid);
  return false;
}

bool ThreadState::SetupThread(int tid) {
  std::map<int, struct traced_child>::iterator it = thread_state_->find(tid);
  if (ThreadNeedsSetup(tid)) {
    if (ptrace(PTRACE_SETOPTIONS, tid, 0,
               PTRACE_O_TRACESYSGOOD |
               PTRACE_O_TRACECLONE |
               PTRACE_O_TRACEFORK |
               PTRACE_O_TRACEVFORK |
               PTRACE_O_TRACEEXEC) != 0) {
      perror("ptrace setoptions");
      return false;
    }
    it->second.needs_setup = false;
#if NACL_SANDBOX_DEBUG > 1
    printf("Setting up thread %d\n", tid);
#endif
  }
  return true;
}

const bool ThreadState::UnknownThread(int tid) {
  std::map<int, struct traced_child>::iterator it = thread_state_->find(tid);
  return it == thread_state_->end();
}

const bool ThreadState::AllDone() {
  return thread_state_->empty();
}

const void ThreadState::DumpThreadState() {
  // dump thread state
  printf("Dumping thread state:\n");
  for (std::map<int,struct traced_child>::const_iterator it =
           thread_state_->begin();
       it != thread_state_->end(); it++) {
    printf("   Thread %d\t%d\n", it->first, it->second.in_syscall);
  }
}


NaclSandbox::NaclSandbox()
    : filter_(NULL) {
  threads_ = new ThreadState();
}

NaclSandbox::~NaclSandbox() {
  if (filter_)
    delete filter_;
  filter_ = NULL;
  if (threads_)
    delete threads_;
  threads_ = NULL;
}

bool NaclSandbox::SelLdrPath(int argc,
                             char** argv,
                             char* path,
                             int path_size) {
  if (argc <= 0) return false;
  char* tmp = strrchr(argv[0], '.');
  if ((NULL == tmp) || (0 != strcmp(tmp, kSelLdrPathSuffix))) {
    fprintf(stderr, "Sandbox must end in .trace\n");
    return false;
  }
  int sel_ldr_path_size = strlen(argv[0]) - strlen(kSelLdrPathSuffix);
  if (sel_ldr_path_size <= 0) {
    fprintf(stderr, "Invalid path name\n");
    return false;
  }
  int end_path = (int)fmin(sel_ldr_path_size, path_size-1);
  path[end_path] = '\0';
  return true;
}

// TODO: Not 64-bit compatible.
int NaclSandbox::KillChildProcess(pid_t pid,
                                  struct user_regs_struct* regs_old,
                                  struct user_regs_struct* regs_new) {
  printf("Killing process %d\n", pid);
  *regs_new = *regs_old;
  regs_new->orig_eax = __NR_exit_group;
  regs_new->eax = __NR_exit_group;
  regs_new->ebx = 0;
  regs_new->ecx = 0;
  regs_new->edx = 0;
  regs_new->eip = 0;
  if (ptrace(PTRACE_SETREGS, pid, NULL, regs_new) != 0) {
    // Sometimes the process has disappeared before we can set the registers.
    perror("set registers");
  }
  // ptrace(PTRACE_KILL ...) won't kill all threads. SIGKILL is harsh
  // but SIGTERM can be ignored.  */
  if (kill(pid, SIGKILL) != 0) {
    perror("kill");
  }
  return 0;
}

bool NaclSandbox::ParseApplicationName(int argc, char** argv, char* app_name,
                                       int buffer_size) {
  int opt;
  while ((opt = getopt(argc, argv, "f:")) != -1) {
    switch (opt) {
      case 'f':
        strncpy(app_name, optarg, buffer_size);
        if (app_name[buffer_size - 1] != '\0') {
          fprintf(stderr, "NaclSandbox: nacl module name too long\n");
          return false;
        }
        return true;
      default:
        break;
    }
  }
  fprintf(stderr,
          "NaclSandbox: sel_ldr called without nacl module\n");
  return false;
}

void NaclSandbox::ExecTracedSelLdr(char* sel_ldr_path, char** argv) {
  // Start (exec) sel_ldr with the given application and arguments.
  int ret_val = ptrace(PTRACE_TRACEME, 0, 0, 0);
  if (ret_val != 0) {
    perror("ptrace traceme");
    exit(-20);
  }
  execv(sel_ldr_path, argv);
  perror("exec");
  exit(-20);
}

void NaclSandbox::Run(char* application_name, char** argv) {
  int wait_val;           /*  child's return value        */
  pid_t pid;                /*  child's process             */
  pid_t tid;                /*  stopped thread              */
  struct user_regs_struct regs, regs_new;
  char* sel_ldr_path = argv[0];

  filter_ = new NaclSyscallFilter(application_name);

  printf("Starting sel_ldr: %s \n", sel_ldr_path);
  switch (pid = fork()) {
    case -1:
      perror("fork");
      break;
    case 0:
      // TODO(neha): make sure this is set in the sel_ldr as well.
      prctl(PR_SET_PDEATHSIG, SIGKILL);
      ExecTracedSelLdr(sel_ldr_path, argv);
      break;
    default:
      // The first call should be the return from exec.
      if (wait4(-1, &wait_val, __WALL, NULL) != pid) {
        if (errno == EINTR) {
          printf("EINTR\n");
          // What to do here? process got an unblocked signal really fast.
        } else {
          KillChildProcess(pid, &regs, &regs_new);
          perror("wait");
          break;
        }
      }
      if (WIFEXITED(wait_val) || WIFSIGNALED(wait_val)) {
        fprintf(stderr, "sel_ldr coming back from wait in non-stopped state\n");
        break;
      }
      if (!WIFSTOPPED(wait_val)) {
        KillChildProcess(pid, &regs, &regs_new);
        fprintf(stderr, "sel_ldr coming back from wait in non-stopped state\n");
        break;
      }
      // The first return from ptrace(PTRACE_SYSCALL, ...) will be
      // upon entry to syscall; thereafter it toggles between entry
      // and exit.
      threads_->AddThread(pid);
      if (!threads_->SetupThread(pid)) {
        KillChildProcess(pid, &regs, &regs_new);
        printf("SetupThread failed\n");
        break;
      }
      int signal = 0;
      bool exited = false;
      tid = pid;
      uintptr_t expected_cs = 0;
      if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) != 0) {
        KillChildProcess(pid, &regs, &regs_new);
        perror("get regs for CS");
        break;
      }
      expected_cs = Registers::GetCS(regs);
      while(true) {
        if (threads_->AllDone()) {
          break;
        }
        if (!exited) {
          if (ptrace(PTRACE_SYSCALL, tid, NULL, signal) != 0) {
            printf("bad ptrace thread %d\n", tid);
            KillChildProcess(pid, &regs, &regs_new);
            perror("ptrace syscall");
            break;
          }
        }
        signal = 0;
        if ((tid = wait4(-1, &wait_val, __WALL, NULL)) < 0) {
          if (errno == EINTR) {
            printf("Supervisor caught signal: EINTR\n");
            continue;
          } else {
            KillChildProcess(pid, &regs, &regs_new);
            perror("wait");
            break;
          }
        }
        if (threads_->UnknownThread(tid)) {
          KillChildProcess(pid, &regs, &regs_new);
          printf("Unknown thread %d\n", tid);
          break;
        }
        if (WIFEXITED(wait_val)) {
          threads_->RemoveThread(tid);
          exited = true;
          continue;
        }
        exited = false;
        // The kernel serializes syscalls, so all following PTRACE
        // actions apply to the thread that was caught by the previous
        // wait4.
        if (WIFSTOPPED(wait_val) && WSTOPSIG(wait_val) == SIGSTOP) {
          // TODO: More checking to see if this is a bogus SIGSTOP
          // (see sandboxsupervisor.cc).
          // Newly created thread.
          threads_->SetupThread(tid);
          continue;
        } else if (WIFSIGNALED(wait_val)) {
          signal = WTERMSIG(wait_val);
          continue;
        } else if (WIFSTOPPED(wait_val) &&
                   (WSTOPSIG(wait_val) == SIGTRAP)) {
          // Check for clone()
          unsigned int event;
          event = (wait_val >> 16) & 0xffff;
          if (event == PTRACE_EVENT_FORK
              || event == PTRACE_EVENT_CLONE
              || event == PTRACE_EVENT_VFORK) {
            int ktid;
            if (ptrace(PTRACE_GETEVENTMSG, tid, 0, &ktid) != 0) {
              KillChildProcess(pid, &regs, &regs_new);
              perror("ptrace thread");
              break;
            }
            // Add the new kid (setup will be done when it wakes up)
            threads_->AddThread(ktid);
            continue;
          } else {
            KillChildProcess(pid, &regs, &regs_new);
          }
        } else if (!WIFSTOPPED(wait_val) ||
                   WSTOPSIG(wait_val) != (SIGTRAP | 0x80)) {
          printf("Some other unhandled signal %08x\n", wait_val);
          KillChildProcess(pid, &regs, &regs_new);
        }
        if (ptrace(PTRACE_GETREGS, tid, NULL, &regs) != 0) {
          KillChildProcess(pid, &regs, &regs_new);
          perror("get regs");
          break;
        }
        bool error = false;
        bool entering = threads_->ThreadEnteringSyscall(tid, &regs, &error);
        if (error) {
           printf("Thread state mismatch\n");
           KillChildProcess(pid, &regs, &regs_new);
           break;
        }
        // Check to make sure there are no 32-bit shenanigans.  The
        // expected_cs value we got above is for 32-bit mode, since it
        // is before the nexe was opened and the sel_ldr is compiled
        // with -m32.
        uintptr_t found_cs  = Registers::GetCS(regs);
        if (expected_cs != found_cs) {
          KillChildProcess(pid, &regs, &regs_new);
          printf("Process changed CS from 32-bit old CS: %08x new CS: %08x\n",
                 expected_cs, found_cs);
          break;
        }
        if (!filter_->RunRule(tid, &regs, entering)) {
          KillChildProcess(pid, &regs, &regs_new);
          printf("Rule execution failed\n");
          break;
        }
        if (!threads_->ChangeThreadSyscallStatus(tid, &regs)) {
          KillChildProcess(pid, &regs, &regs_new);
          break;
        }
      }
      // When debugging only.
      // filter_->DebugPrintBadSyscalls();
  }
}
