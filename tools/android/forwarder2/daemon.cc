// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/daemon.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/safe_strerror_posix.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "tools/android/forwarder2/common.h"

namespace forwarder2 {
namespace {

const char kLogFilePath[] = "/tmp/host_forwarder_log";

class FileDescriptorAutoCloser {
 public:
  explicit FileDescriptorAutoCloser(int fd) : fd_(fd) {
    DCHECK(fd_ >= 0);
  }

  ~FileDescriptorAutoCloser() {
    if (fd_ > -1)
      CloseFD(fd_);
  }

  int Release() {
    const int fd = fd_;
    fd_ = -1;
    return fd;
  }

 private:
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(FileDescriptorAutoCloser);
};

// Handles creation and destruction of the PID file.
class PIDFile {
 public:
  static scoped_ptr<PIDFile> Create(const std::string& path) {
    scoped_ptr<PIDFile> pid_file;
    const int pid_file_fd = HANDLE_EINTR(
        open(path.c_str(), O_CREAT | O_WRONLY, 0600));
    if (pid_file_fd < 0) {
      PError("open()");
      return pid_file.Pass();
    }
    FileDescriptorAutoCloser fd_closer(pid_file_fd);
    struct flock lock_info = {};
    lock_info.l_type = F_WRLCK;
    lock_info.l_whence = SEEK_CUR;
    if (HANDLE_EINTR(fcntl(pid_file_fd, F_SETLK, &lock_info)) < 0) {
      if (errno == EAGAIN || errno == EACCES) {
        LOG(ERROR) << "Daemon already running (PID file already locked)";
        return pid_file.Pass();
      }
      PError("lockf()");
      return pid_file.Pass();
    }
    const std::string pid_string = base::StringPrintf("%d\n", getpid());
    CHECK(HANDLE_EINTR(write(pid_file_fd, pid_string.c_str(),
                             pid_string.length())));
    pid_file.reset(new PIDFile(fd_closer.Release(), path));
    return pid_file.Pass();
  }

  ~PIDFile() {
    CloseFD(fd_);  // This also releases the lock.
    if (remove(path_.c_str()) < 0)
      PError("remove");
  }

 private:
  PIDFile(int fd, const std::string& path) : fd_(fd), path_(path) {
    DCHECK(fd_ >= 0);
  }

  const int fd_;
  const std::string path_;

  DISALLOW_COPY_AND_ASSIGN(PIDFile);
};

// Takes ownership of |data|.
void ReleaseDaemonResourcesAtExit(void* data) {
  DCHECK(data);
  delete reinterpret_cast<PIDFile*>(data);
}

void InitLogging(const char* log_file) {
  CHECK(
      logging::InitLogging(
          log_file,
          logging::LOG_ONLY_TO_FILE,
          logging::DONT_LOCK_LOG_FILE,
          logging::APPEND_TO_OLD_LOG_FILE,
          logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS));
}

void SigChildHandler(int signal_number) {
  DCHECK_EQ(signal_number, SIGCHLD);
  // The daemon should not terminate while its parent is still running.
  int status;
  pid_t child_pid = waitpid(-1 /* any child */, &status, WNOHANG);
  if (child_pid < 0) {
    PError("waitpid");
    return;
  }
  if (child_pid == 0)
    return;
  // Avoid using StringAppendF() since it's unsafe in a signal handler due to
  // its use of LOG().
  FixedSizeStringBuilder<256> string_builder;
  string_builder.Append("Daemon (pid=%d) died unexpectedly with ", child_pid);
  if (WIFEXITED(status))
    string_builder.Append("status %d.", WEXITSTATUS(status));
  else if (WIFSIGNALED(status))
    string_builder.Append("signal %d.", WTERMSIG(status));
  else
    string_builder.Append("unknown reason.");
  SIGNAL_SAFE_LOG(ERROR, string_builder.buffer());
}

// Note that 0 is written to |lock_owner_pid| in case the file is not locked.
bool GetFileLockOwnerPid(int fd, pid_t* lock_owner_pid) {
  struct flock lock_info = {};
  lock_info.l_type = F_WRLCK;
  lock_info.l_whence = SEEK_CUR;
  const int ret = HANDLE_EINTR(fcntl(fd, F_GETLK, &lock_info));
  if (ret < 0) {
    if (errno == EBADF) {
      // Assume that the provided file descriptor corresponding to the PID file
      // was valid until the daemon removed this file.
      *lock_owner_pid = 0;
      return true;
    }
    PError("fcntl");
    return false;
  }
  if (lock_info.l_type == F_UNLCK) {
    *lock_owner_pid = 0;
    return true;
  }
  CHECK_EQ(F_WRLCK /* exclusive lock */, lock_info.l_type);
  *lock_owner_pid = lock_info.l_pid;
  return true;
}

}  // namespace

Daemon::Daemon(const std::string& pid_file_path)
    : pid_file_path_(pid_file_path) {
}

bool Daemon::Spawn(bool* is_daemon) {
  switch (fork()) {
    case -1:
      *is_daemon = false;
      PError("fork()");
      return false;
    case 0: {  // Child.
      *is_daemon = true;
      scoped_ptr<PIDFile> pid_file = PIDFile::Create(pid_file_path_);
      if (!pid_file)
        return false;
      base::AtExitManager::RegisterCallback(
          &ReleaseDaemonResourcesAtExit, pid_file.release());
      if (setsid() < 0) {  // Detach the child process from its parent.
        PError("setsid");
        return false;
      }
      CloseFD(STDOUT_FILENO);
      CloseFD(STDERR_FILENO);
      InitLogging(kLogFilePath);
      break;
    }
    default:  // Parent.
      *is_daemon = false;
      signal(SIGCHLD, SigChildHandler);
  }
  return true;
}

bool Daemon::Kill() {
  int pid_file_fd = HANDLE_EINTR(open(pid_file_path_.c_str(), O_WRONLY));
  if (pid_file_fd < 0) {
    if (errno == ENOENT)
      return true;
    LOG(ERROR) << "Could not open " << pid_file_path_ << " in write mode: "
               << safe_strerror(errno);
    return false;
  }
  const FileDescriptorAutoCloser fd_closer(pid_file_fd);
  pid_t lock_owner_pid;
  if (!GetFileLockOwnerPid(pid_file_fd, &lock_owner_pid))
    return false;
  if (lock_owner_pid == 0)
    // No daemon running.
    return true;
  if (kill(lock_owner_pid, SIGTERM) < 0) {
    if (errno == ESRCH /* invalid PID */)
      // The daemon exited for some reason (e.g. kill by a process other than
      // us) right before the call to kill() above.
      return true;
    PError("kill");
    return false;
  }
  // Wait until the daemon exits. Rely on the fact that the daemon releases the
  // lock on the PID file when it exits.
  // TODO(pliard): Consider using a mutex + condition in shared memory to avoid
  // polling.
  const int kTries = 20;
  const int kIdleTimeMS = 50;
  for (int i = 0; i < kTries; ++i) {
    pid_t current_lock_owner_pid;
    if (!GetFileLockOwnerPid(pid_file_fd, &current_lock_owner_pid))
      return false;
    if (current_lock_owner_pid == 0)
      // The daemon released the PID file's lock.
      return true;
    // Since we are polling we might not see the 'daemon exited' event if
    // another daemon was spawned during our idle period.
    if (current_lock_owner_pid != lock_owner_pid) {
      LOG(WARNING) << "Daemon (pid=" << lock_owner_pid
                   << ") was successfully killed but a new daemon (pid="
                   << current_lock_owner_pid << ") seems to be running now.";
      return true;
    }
    usleep(kIdleTimeMS * 1000);
  }
  LOG(ERROR) << "Timed out while killing daemon. "
                "It might still be tearing down.";
  return false;
}

}  // namespace forwarder2
