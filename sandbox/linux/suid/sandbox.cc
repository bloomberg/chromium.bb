// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox

#include <asm/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if !defined(CLONE_NEWPID)
#define CLONE_NEWPID 0x20000000
#endif

static const char kChromeBinary[] = "/opt/google/chrome/chrome";
static const char kSandboxDescriptorEnvironmentVarName[] = "SBX_D";

// These are the magic byte values which the sandboxed process uses to request
// that it be chrooted.
static const char kMsgChrootMe = 'C';
static const char kMsgChrootSuccessful = 'O';

static void FatalError(const char *msg, ...)
    __attribute__((noreturn, format(printf,1,2)));

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

  const pid_t pid = syscall(
      __NR_clone, CLONE_FS | SIGCHLD, 0, 0, 0);

  if (pid == -1) {
    perror("clone");
    close(sv[0]);
    close(sv[1]);
    return -1;
  }

  if (pid == 0) {
    // We create a temp directory for our chroot. Nobody should ever write into
    // it, so it's root:root 0555.
    char kTempDirectoryTemplate[] = "/tmp/chrome-sandbox-chroot-XXXXXX";
    const char* temp_dir = mkdtemp(kTempDirectoryTemplate);
    if (!temp_dir)
      FatalError("Failed to create temp directory for chroot");

    const int chroot_dir_fd = open(temp_dir, O_DIRECTORY | O_RDONLY);
    if (chroot_dir_fd < 0) {
      rmdir(temp_dir);
      FatalError("Failed to open chroot temp directory");
    }

    rmdir(temp_dir);
    fchown(chroot_dir_fd, 0 /* root */, 0 /* root */);

    // We share our files structure with an untrusted process. As a security in
    // depth measure, we make sure that we can't open anything by mistake.
    // TODO: drop CAP_SYS_RESOURCE

    const struct rlimit nofile = {0, 0};
    if (setrlimit(RLIMIT_NOFILE, &nofile))
      FatalError("Setting RLIMIT_NOFILE");

    if (close(sv[1]))
      FatalError("close");

    char msg;
    ssize_t bytes;
    do {
      bytes = read(sv[0], &msg, 1);
    } while (bytes == -1 && errno == EINTR);

    if (bytes == 0)
      _exit(0);
    if (bytes != 1)
      FatalError("read");

    if (msg != kMsgChrootMe)
      FatalError("Unknown message from sandboxed process");

    if (fchdir(chroot_dir_fd))
      FatalError("Cannot chdir into chroot temp directory");
    fchmod(chroot_dir_fd, 0000 /* no-access */);

    struct stat st;
    if (stat(".", &st))
      FatalError("stat");

    if (st.st_uid || st.st_gid || st.st_mode & S_IWOTH)
      FatalError("Bad permissions on chroot temp directory");

    if (chroot("."))
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
  snprintf(desc_str, sizeof(desc_str), "%d", chroot_signal_fd);

  if (setenv(kSandboxDescriptorEnvironmentVarName, desc_str, 1)) {
    perror("setenv");
    close(chroot_signal_fd);
    return false;
  }

  return true;
}

static bool MoveToNewPIDNamespace() {
  const pid_t pid = syscall(
      __NR_clone, CLONE_NEWPID | SIGCHLD, 0, 0, 0);

  if (pid == -1) {
    if (errno == EINVAL) {
      // System doesn't support NEWPID. We carry on anyway.
      return true;
    }

    perror("Failed to move to new PID namespace");
    return false;
  }

  if (pid)
    _exit(0);

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

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "Usage: %s <renderer process> <args...>\n", argv[0]);
    return 1;
  }

#if defined(DEVELOPMENT_SANDBOX)
  // On development machines, we need the sandbox to be able to run development
  // builds of Chrome. Thus, we remove the condition that the path to the
  // binary has to be fixed. However, we still worry about running arbitary
  // executables like this so we require that the owner of the binary be the
  // same as the real UID.
  const int binary_fd = open(argv[1], O_RDONLY);
  if (binary_fd < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
    return 1;
  }

  struct stat st;
  if (fstat(binary_fd, &st)) {
    fprintf(stderr, "Failed to stat %s: %s\n", argv[1], strerror(errno));
    return 1;
  }

  if (fcntl(binary_fd, F_SETFD, O_CLOEXEC)) {
    fprintf(stderr, "Failed to set close-on-exec flag: %s", strerror(errno));
    return 1;
  }

  uid_t ruid, euid, suid;
  getresuid(&ruid, &euid, &suid);

  if (st.st_uid != ruid) {
    fprintf(stderr, "The development sandbox is refusing to run %s because it "
                    "isn't owned by the current user (%d)\n", argv[1], ruid);
    return 1;
  }

  char proc_fd_buffer[128];
  snprintf(proc_fd_buffer, sizeof(proc_fd_buffer), "/proc/self/fd/%d", binary_fd);
  argv[1] = proc_fd_buffer;
#else
  // In the release sandbox, we'll only execute a specific path.
  if (strcmp(argv[1], kChromeBinary)) {
    fprintf(stderr, "This wrapper can only run %s!\n", kChromeBinary);
    fprintf(stderr, "If you are developing Chrome, you should read:\n"
        "http://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment\n");
    return 1;
  }
#endif

  if (!MoveToNewPIDNamespace())
    return 1;
  if (!SpawnChrootHelper())
    return 1;
  if (!DropRoot())
    return 1;

  execv(argv[1], &argv[1]);
  FatalError("execv failed");

  return 1;
}
