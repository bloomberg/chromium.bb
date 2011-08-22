// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox

#define _GNU_SOURCE
#include <asm/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "linux_util.h"
#include "process_util.h"
#include "suid_unsafe_environment_variables.h"

#if !defined(CLONE_NEWPID)
#define CLONE_NEWPID 0x20000000
#endif
#if !defined(CLONE_NEWNET)
#define CLONE_NEWNET 0x40000000
#endif

static const char kAdjustOOMScoreSwitch[] = "--adjust-oom-score";
static const char kSandboxDescriptorEnvironmentVarName[] = "SBX_D";
static const char kSandboxHelperPidEnvironmentVarName[] = "SBX_HELPER_PID";

// These are the magic byte values which the sandboxed process uses to request
// that it be chrooted.
static const char kMsgChrootMe = 'C';
static const char kMsgChrootSuccessful = 'O';

static void FatalError(const char *msg, ...)
    __attribute__((noreturn, format(printf, 1, 2)));

static void FatalError(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);

  vfprintf(stderr, msg, ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  fflush(stderr);
  exit(1);
}

// We will chroot() to the helper's /proc/self directory. Anything there will
// not exist anymore if we make sure to wait() for the helper.
//
// /proc/self/fdinfo or /proc/self/fd are especially safe and will be empty
// even if the helper survives as a zombie.
//
// There is very little reason to use fdinfo/ instead of fd/ but we are
// paranoid. fdinfo/ only exists since 2.6.22 so we allow fallback to fd/
#define SAFE_DIR "/proc/self/fdinfo"
#define SAFE_DIR2 "/proc/self/fd"

static bool SpawnChrootHelper() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
    perror("socketpair");
    return false;
  }

  char *safedir = NULL;
  struct stat sdir_stat;
  if (!stat(SAFE_DIR, &sdir_stat) && S_ISDIR(sdir_stat.st_mode))
    safedir = SAFE_DIR;
  else
    if (!stat(SAFE_DIR2, &sdir_stat) && S_ISDIR(sdir_stat.st_mode))
      safedir = SAFE_DIR2;
    else {
      fprintf(stderr, "Could not find %s\n", SAFE_DIR2);
      return false;
    }

  const pid_t pid = syscall(
      __NR_clone, CLONE_FS | SIGCHLD, 0, 0, 0);

  if (pid == -1) {
    perror("clone");
    close(sv[0]);
    close(sv[1]);
    return false;
  }

  if (pid == 0) {
    // We share our files structure with an untrusted process. As a security in
    // depth measure, we make sure that we can't open anything by mistake.
    // TODO(agl): drop CAP_SYS_RESOURCE / use SECURE_NOROOT

    const struct rlimit nofile = {0, 0};
    if (setrlimit(RLIMIT_NOFILE, &nofile))
      FatalError("Setting RLIMIT_NOFILE");

    if (close(sv[1]))
      FatalError("close");

    // wait for message
    char msg;
    ssize_t bytes;
    do {
      bytes = read(sv[0], &msg, 1);
    } while (bytes == -1 && errno == EINTR);

    if (bytes == 0)
      _exit(0);
    if (bytes != 1)
      FatalError("read");

    // do chrooting
    if (msg != kMsgChrootMe)
      FatalError("Unknown message from sandboxed process");

    // sanity check
    if (chdir(safedir))
      FatalError("Cannot chdir into /proc/ directory");

    if (chroot(safedir))
      FatalError("Cannot chroot into /proc/ directory");

    if (chdir("/"))
      FatalError("Cannot chdir to / after chroot");

    const char reply = kMsgChrootSuccessful;
    do {
      bytes = write(sv[0], &reply, 1);
    } while (bytes == -1 && errno == EINTR);

    if (bytes != 1)
      FatalError("Writing reply");

    _exit(0);
    // We now become a zombie. /proc/self/fd(info) is now an empty dir and we
    // are chrooted there.
    // Our (unprivileged) parent should not even be able to open "." or "/"
    // since they would need to pass the ptrace() check. If our parent wait()
    // for us, our root directory will completely disappear.
  }

  if (close(sv[0])) {
    close(sv[1]);
    perror("close");
    return false;
  }

  // In the parent process, we install an environment variable containing the
  // number of the file descriptor.
  char desc_str[64];
  int printed = snprintf(desc_str, sizeof(desc_str), "%u", sv[1]);
  if (printed < 0 || printed >= (int)sizeof(desc_str)) {
    fprintf(stderr, "Failed to snprintf\n");
    return false;
  }

  if (setenv(kSandboxDescriptorEnvironmentVarName, desc_str, 1)) {
    perror("setenv");
    close(sv[1]);
    return false;
  }

  // We also install an environment variable containing the pid of the child
  char helper_pid_str[64];
  printed = snprintf(helper_pid_str, sizeof(helper_pid_str), "%u", pid);
  if (printed < 0 || printed >= (int)sizeof(helper_pid_str)) {
    fprintf(stderr, "Failed to snprintf\n");
    return false;
  }

  if (setenv(kSandboxHelperPidEnvironmentVarName, helper_pid_str, 1)) {
    perror("setenv");
    close(sv[1]);
    return false;
  }

  return true;
}

static bool MoveToNewNamespaces() {
  // These are the sets of flags which we'll try, in order.
  const int kCloneExtraFlags[] = {
    CLONE_NEWPID | CLONE_NEWNET,
    CLONE_NEWPID,
  };

  for (size_t i = 0;
       i < sizeof(kCloneExtraFlags) / sizeof(kCloneExtraFlags[0]);
       i++) {
    pid_t pid = syscall(__NR_clone, SIGCHLD | kCloneExtraFlags[i], 0, 0, 0);

    if (pid > 0)
      _exit(0);

    if (pid == 0) {
      if (kCloneExtraFlags[i] & CLONE_NEWPID) {
        setenv("SBX_PID_NS", "", 1 /* overwrite */);
      } else {
        unsetenv("SBX_PID_NS");
      }

      if (kCloneExtraFlags[i] & CLONE_NEWNET) {
        setenv("SBX_NET_NS", "", 1 /* overwrite */);
      } else {
        unsetenv("SBX_NET_NS");
      }

      break;
    }

    if (errno != EINVAL) {
      perror("Failed to move to new PID namespace");
      return false;
    }
  }

  // If the system doesn't support NEWPID then we carry on anyway.
  return true;
}

static bool DropRoot() {
  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0)) {
    perror("prctl(PR_SET_DUMPABLE)");
    return false;
  }

  if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)) {
    perror("Still dumpable after prctl(PR_SET_DUMPABLE)");
    return false;
  }

  gid_t rgid, egid, sgid;
  if (getresgid(&rgid, &egid, &sgid)) {
    perror("getresgid");
    return false;
  }

  if (setresgid(rgid, rgid, rgid)) {
    perror("setresgid");
    return false;
  }

  uid_t ruid, euid, suid;
  if (getresuid(&ruid, &euid, &suid)) {
    perror("getresuid");
    return false;
  }

  if (setresuid(ruid, ruid, ruid)) {
    perror("setresuid");
    return false;
  }

  return true;
}

static bool SetupChildEnvironment() {
  unsigned i;

  // ld.so may have cleared several environment variables because we are SUID.
  // However, the child process might need them so zygote_host_linux.cc saves a
  // copy in SANDBOX_$x. This is safe because we have dropped root by this
  // point, so we can only exec a binary with the permissions of the user who
  // ran us in the first place.

  for (i = 0; kSUIDUnsafeEnvironmentVariables[i]; ++i) {
    const char* const envvar = kSUIDUnsafeEnvironmentVariables[i];
    char* const saved_envvar = SandboxSavedEnvironmentVariable(envvar);
    if (!saved_envvar)
      return false;

    const char* const value = getenv(saved_envvar);
    if (value) {
      setenv(envvar, value, 1 /* overwrite */);
      unsetenv(saved_envvar);
    }

    free(saved_envvar);
  }

  return true;
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <renderer process> <args...>\n", argv[0]);
    return 1;
  }

  // In the SUID sandbox, if we succeed in calling MoveToNewNamespaces()
  // below, then the zygote and all the renderers are in an alternate PID
  // namespace and do not know their real PIDs. As such, they report the wrong
  // PIDs to the task manager.
  //
  // To fix this, when the zygote spawns a new renderer, it gives the renderer
  // a dummy socket, which has a unique inode number. Then it asks the sandbox
  // host to find the PID of the process holding that fd by searching /proc.
  //
  // Since the zygote and renderers are all spawned by this setuid executable,
  // their entries in /proc are owned by root and only readable by root. In
  // order to search /proc for the fd we want, this setuid executable has to
  // double as a helper and perform the search. The code block below does this
  // when you call it with --find-inode INODE_NUMBER.
  if (argc == 3 && (0 == strcmp(argv[1], kFindInodeSwitch))) {
    pid_t pid;
    char* endptr;
    ino_t inode = strtoull(argv[2], &endptr, 10);
    if (inode == ULLONG_MAX || *endptr)
      return 1;
    if (!FindProcessHoldingSocket(&pid, inode))
      return 1;
    printf("%d\n", pid);
    return 0;
  }
  // Likewise, we cannot adjust /proc/pid/oom_adj for sandboxed renderers
  // because those files are owned by root. So we need another helper here.
  if (argc == 4 && (0 == strcmp(argv[1], kAdjustOOMScoreSwitch))) {
    char* endptr;
    long score;
    unsigned long pid_ul = strtoul(argv[2], &endptr, 10);
    if (pid_ul == ULONG_MAX || *endptr)
      return 1;
    pid_t pid = pid_ul;
    score = strtol(argv[3], &endptr, 10);
    if (score == LONG_MAX || score == LONG_MIN || *endptr)
      return 1;
    return AdjustOOMScore(pid, score);
  }

  if (!MoveToNewNamespaces())
    return 1;
  if (!SpawnChrootHelper())
    return 1;
  if (!DropRoot())
    return 1;
  if (!SetupChildEnvironment())
    return 1;

  execv(argv[1], &argv[1]);
  FatalError("execv failed");

  return 1;
}
