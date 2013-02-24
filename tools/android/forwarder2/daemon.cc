// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/daemon.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/safe_strerror_posix.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "tools/android/forwarder2/common.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {
namespace {

const int kBufferSize = 256;

// Timeout constant used for polling when connecting to the daemon's Unix Domain
// Socket and also when waiting for its death when it is killed.
const int kNumTries = 100;
const int kIdleTimeMSec = 20;

void InitLoggingForDaemon(const std::string& log_file) {
  CHECK(
      logging::InitLogging(
          log_file.c_str(),
          log_file.empty() ?
              logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG : logging::LOG_ONLY_TO_FILE,
          logging::DONT_LOCK_LOG_FILE, logging::APPEND_TO_OLD_LOG_FILE,
          logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS));
}

bool RunServerAcceptLoop(const std::string& welcome_message,
                         Socket* server_socket,
                         Daemon::ServerDelegate* server_delegate) {
  bool failed = false;
  for (;;) {
    scoped_ptr<Socket> client_socket(new Socket());
    if (!server_socket->Accept(client_socket.get())) {
      if (server_socket->exited())
        break;
      PError("Accept()");
      failed = true;
      break;
    }
    if (!client_socket->Write(welcome_message.c_str(),
                              welcome_message.length() + 1)) {
      PError("Write()");
      failed = true;
      continue;
    }
    server_delegate->OnClientConnected(client_socket.Pass());
  }
  server_delegate->OnServerExited();
  return !failed;
}

void SigChildHandler(int signal_number) {
  DCHECK_EQ(signal_number, SIGCHLD);
  int status;
  pid_t child_pid = waitpid(-1 /* any child */, &status, WNOHANG);
  if (child_pid < 0) {
    PError("waitpid");
    return;
  }
  if (child_pid == 0)
    return;
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
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

scoped_ptr<Socket> ConnectToUnixDomainSocket(
    const std::string& socket_name,
    int tries_count,
    int idle_time_msec,
    const std::string& expected_welcome_message) {
  for (int i = 0; i < tries_count; ++i) {
    scoped_ptr<Socket> socket(new Socket());
    if (!socket->ConnectUnix(socket_name, true)) {
      if (idle_time_msec)
        usleep(idle_time_msec * 1000);
      continue;
    }
    char buf[kBufferSize];
    DCHECK(expected_welcome_message.length() + 1 <= sizeof(buf));
    memset(buf, 0, sizeof(buf));
    if (socket->Read(buf, sizeof(buf)) < 0) {
      perror("read");
      continue;
    }
    if (expected_welcome_message != buf) {
      LOG(ERROR) << "Unexpected message read from daemon: " << buf;
      break;
    }
    return socket.Pass();
  }
  return scoped_ptr<Socket>(NULL);
}

}  // namespace

// Handles creation and destruction of the PID file.
class Daemon::PIDFile {
 public:
  static bool Create(const std::string& path, scoped_ptr<PIDFile>* pid_file) {
    int pid_file_fd = HANDLE_EINTR(
        open(path.c_str(), O_CREAT | O_WRONLY, 0666));
    if (pid_file_fd < 0) {
      PError("open()");
      return false;
    }
    file_util::ScopedFD fd_closer(&pid_file_fd);
    struct flock lock_info = {};
    lock_info.l_type = F_WRLCK;
    lock_info.l_whence = SEEK_CUR;
    if (HANDLE_EINTR(fcntl(pid_file_fd, F_SETLK, &lock_info)) < 0) {
      if (errno == EAGAIN || errno == EACCES) {
        LOG(INFO) << "Daemon already running (PID file already locked)";
        // Don't consider this case as a failure. This can happen when trying to
        // spawn multiple daemons concurrently.
        return true;
      }
      PError("lockf()");
      return false;
    }
    const std::string pid_string = base::StringPrintf("%d\n", getpid());
    CHECK(HANDLE_EINTR(write(pid_file_fd, pid_string.c_str(),
                             pid_string.length())));
    pid_file->reset(new PIDFile(*fd_closer.release(), path));
    return true;
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

Daemon::Daemon(const std::string& log_file_path,
               const std::string& pid_file_path,
               const std::string& identifier,
               ClientDelegate* client_delegate,
               ServerDelegate* server_delegate,
               GetExitNotifierFDCallback get_exit_fd_callback)
  : log_file_path_(log_file_path),
    pid_file_path_(pid_file_path),
    identifier_(identifier),
    client_delegate_(client_delegate),
    server_delegate_(server_delegate),
    get_exit_fd_callback_(get_exit_fd_callback) {
  DCHECK(client_delegate_);
  DCHECK(server_delegate_);
  DCHECK(get_exit_fd_callback_);
}

Daemon::~Daemon() {}

bool Daemon::SpawnIfNeeded() {
  const int kSingleTry = 1;
  const int kNoIdleTime = 0;
  scoped_ptr<Socket> client_socket = ConnectToUnixDomainSocket(
      identifier_, kSingleTry, kNoIdleTime, identifier_);
  if (!client_socket) {
    switch (fork()) {
      case -1:
        PError("fork()");
        return false;
      // Child.
      case 0: {
        DCHECK(!pid_file_);
        if (!PIDFile::Create(pid_file_path_, &pid_file_))
          exit(1);
        if (!pid_file_.get())  // Another daemon was spawn concurrently.
          exit(0);
        if (setsid() < 0) {  // Detach the child process from its parent.
          PError("setsid()");
          exit(1);
        }
        InitLoggingForDaemon(log_file_path_);
        CloseFD(STDIN_FILENO);
        CloseFD(STDOUT_FILENO);
        CloseFD(STDERR_FILENO);
        const int null_fd = open("/dev/null", O_RDWR);
        CHECK_EQ(null_fd, STDIN_FILENO);
        CHECK_EQ(dup(null_fd), STDOUT_FILENO);
        CHECK_EQ(dup(null_fd), STDERR_FILENO);
        Socket command_socket;
        if (!command_socket.BindUnix(identifier_, true)) {
          PError("bind()");
          exit(1);
        }
        server_delegate_->Init();
        command_socket.set_exit_notifier_fd(get_exit_fd_callback_());
        exit(!RunServerAcceptLoop(identifier_, &command_socket,
                                  server_delegate_));
      }
      default:
        break;
    }
  }
  // Parent.
  // Install the custom SIGCHLD handler.
  sigset_t blocked_signals_set;
  if (sigprocmask(0 /* first arg ignored */, NULL, &blocked_signals_set) < 0) {
    PError("sigprocmask()");
    return false;
  }
  struct sigaction old_action;
  struct sigaction new_action;
  memset(&new_action, 0, sizeof(new_action));
  new_action.sa_handler = SigChildHandler;
  new_action.sa_flags = SA_NOCLDSTOP;
  sigemptyset(&new_action.sa_mask);
  if (sigaction(SIGCHLD, &new_action, &old_action) < 0) {
    PError("sigaction()");
    return false;
  }
  // Connect to the daemon's Unix Domain Socket.
  bool failed = false;
  if (!client_socket) {
    client_socket = ConnectToUnixDomainSocket(
        identifier_, kNumTries, kIdleTimeMSec, identifier_);
    if (!client_socket) {
      LOG(ERROR) << "Could not connect to daemon's Unix Daemon socket";
      failed = true;
    }
  }
  if (!failed)
    client_delegate_->OnDaemonReady(client_socket.get());
  // Restore the previous signal action for SIGCHLD.
  if (sigaction(SIGCHLD, &old_action, NULL) < 0) {
    PError("sigaction");
    failed = true;
  }
  return !failed;
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
  const file_util::ScopedFD fd_closer(&pid_file_fd);
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
  for (int i = 0; i < kNumTries; ++i) {
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
    usleep(kIdleTimeMSec * 1000);
  }
  LOG(ERROR) << "Timed out while killing daemon. "
                "It might still be tearing down.";
  return false;
}

}  // namespace forwarder2
