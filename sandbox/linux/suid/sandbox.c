// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#if !defined(CLONE_NEWNS)
#define CLONE_NEWNS 0x00020000
#endif

#if !defined(BTRFS_SUPER_MAGIC)
#define BTRFS_SUPER_MAGIC 0x9123683E
#endif
#if !defined(EXT2_SUPER_MAGIC)
#define EXT2_SUPER_MAGIC 0xEF53
#endif
#if !defined(EXT3_SUPER_MAGIC)
#define EXT3_SUPER_MAGIC 0xEF53
#endif
#if !defined(EXT4_SUPER_MAGIC)
#define EXT4_SUPER_MAGIC 0xEF53
#endif
#if !defined(REISERFS_SUPER_MAGIC)
#define REISERFS_SUPER_MAGIC 0x52654973
#endif
#if !defined(TMPFS_MAGIC)
#define TMPFS_MAGIC 0x01021994
#endif
#if !defined(XFS_SUPER_MAGIC)
#define XFS_SUPER_MAGIC 0x58465342
#endif

static const char kSandboxDescriptorEnvironmentVarName[] = "SBX_D";

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

static int CloneChrootHelperProcess() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
    perror("socketpair");
    return -1;
  }

  // Some people mount /tmp on a non-POSIX filesystem (e.g. NFS). This
  // breaks all sorts of assumption in our code. So, if we don't recognize the
  // filesystem, we will try to use an alternative location for our temp
  // directory.
  char tempDirectoryTemplate[80] = "/tmp/chrome-sandbox-chroot-XXXXXX";
  struct statfs sfs;
  if (!statfs("/tmp", &sfs) &&
      (unsigned long)sfs.f_type != BTRFS_SUPER_MAGIC &&
      (unsigned long)sfs.f_type != EXT2_SUPER_MAGIC &&
      (unsigned long)sfs.f_type != EXT3_SUPER_MAGIC &&
      (unsigned long)sfs.f_type != EXT4_SUPER_MAGIC &&
      (unsigned long)sfs.f_type != REISERFS_SUPER_MAGIC &&
      (unsigned long)sfs.f_type != TMPFS_MAGIC &&
      (unsigned long)sfs.f_type != XFS_SUPER_MAGIC) {
    // If /dev/shm exists, it is supposed to be a tmpfs filesystem. While we
    // are not actually using it for shared memory, moving our temp directory
    // into a known tmpfs filesystem is preferable over using a potentially
    // unreliable non-POSIX filesystem.
    if (!statfs("/dev/shm", &sfs) && sfs.f_type == TMPFS_MAGIC) {
      *tempDirectoryTemplate = '\000';
      strncat(tempDirectoryTemplate, "/dev/shm/chrome-sandbox-chroot-XXXXXX",
              sizeof(tempDirectoryTemplate) - 1);
    } else {
      // Neither /tmp is a well-known POSIX filesystem, nor /dev/shm is a
      // tmpfs. After all, we now use /tmp as the location of our temp
      // directory, but we quite likely fail the moment we try to access it
      // through chroot_dir_fd. If so, we will print a verbose error message
      // (see below)
    }
  }

  // We create a temp directory for our chroot. Nobody should ever write into
  // it, so it's root:root mode 000.
  const char* temp_dir = mkdtemp(tempDirectoryTemplate);
  if (!temp_dir) {
    perror("Failed to create temp directory for chroot");
    return -1;
  }

  const int chroot_dir_fd = open(temp_dir, O_DIRECTORY | O_RDONLY);
  if (chroot_dir_fd < 0) {
    rmdir(temp_dir);
    perror("Failed to open chroot temp directory");
    return -1;
  }

  if (rmdir(temp_dir)) {
    perror("rmdir");
    return -1;
  }

  char proc_self_fd_str[128];
  int printed = snprintf(proc_self_fd_str, sizeof(proc_self_fd_str),
                         "/proc/self/fd/%d", chroot_dir_fd);
  if (printed < 0 || printed >= (int)sizeof(proc_self_fd_str)) {
    fprintf(stderr, "Error in snprintf");
    return -1;
  }

  if (fchown(chroot_dir_fd, 0 /* root */, 0 /* root */)) {
    fprintf(stderr, "Could not set up sandbox work directory. Maybe, /tmp is "
                    "a non-POSIX filesystem and /dev/shm doesn't exist "
                    "either. Consider mounting a \"tmpfs\" on /tmp.\n");
    return -1;
  }

  if (fchmod(chroot_dir_fd, 0000 /* no-access */)) {
    perror("fchmod");
    return -1;
  }


  const pid_t pid = syscall(
      __NR_clone, CLONE_FS | SIGCHLD, 0, 0, 0);

  if (pid == -1) {
    perror("clone");
    close(sv[0]);
    close(sv[1]);
    return -1;
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

    if (fchdir(chroot_dir_fd))
      FatalError("Cannot chdir into chroot temp directory");

    struct stat st;
    if (fstat(chroot_dir_fd, &st))
      FatalError("stat");

    if (st.st_uid || st.st_gid || st.st_mode & 0777)
      FatalError("Bad permissions on chroot temp directory");

    if (chroot(proc_self_fd_str))
      FatalError("Cannot chroot into temp directory");

    if (chdir("/"))
      FatalError("Cannot chdir to / after chroot");

    const char reply = kMsgChrootSuccessful;
    do {
      bytes = write(sv[0], &reply, 1);
    } while (bytes == -1 && errno == EINTR);

    if (bytes != 1)
      FatalError("Writing reply");

    _exit(0);
  }

  if (close(chroot_dir_fd)) {
    close(sv[0]);
    close(sv[1]);
    perror("close(chroot_dir_fd)");
    return false;
  }

  if (close(sv[0])) {
    close(sv[1]);
    perror("close");
    return false;
  }

  return sv[1];
}

static bool SpawnChrootHelper() {
  const int chroot_signal_fd = CloneChrootHelperProcess();

  if (chroot_signal_fd == -1)
    return false;

  // In the parent process, we install an environment variable containing the
  // number of the file descriptor.
  char desc_str[64];
  int printed = snprintf(desc_str, sizeof(desc_str), "%d", chroot_signal_fd);
  if (printed < 0 || printed >= (int)sizeof(desc_str)) {
    fprintf(stderr, "Failed to snprintf\n");
    return false;
  }

  if (setenv(kSandboxDescriptorEnvironmentVarName, desc_str, 1)) {
    perror("setenv");
    close(chroot_signal_fd);
    return false;
  }

  return true;
}

static bool MoveToNewNamespaces() {
  // These are the sets of flags which we'll try, in order.
  const int kCloneExtraFlags[] = {
    CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS,
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

      if (kCloneExtraFlags[i] & CLONE_NEWPID) {
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
