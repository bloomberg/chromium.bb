// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/broker_process.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"

namespace {

static const int kCommandOpen = 'O';
static const size_t kMaxMessageLength = 4096;

// Some flags will need special treatment on the client side and are not
// supported for now.
int ForCurrentProcessFlagsMask() {
  return O_CLOEXEC | O_NONBLOCK;
}

// Check whether |requested_filename| is in |allowed_file_names|.
// See GetFileNameIfAllowedAccess() for an explaination of |file_to_open|.
// async signal safe if |file_to_open| is NULL.
// TODO(jln): assert signal safety.
bool GetFileNameInWhitelist(const std::vector<std::string>& allowed_file_names,
                            const std::string& requested_filename,
                            const char** file_to_open) {
  if (file_to_open && *file_to_open) {
    // Make sure that callers never pass a non-empty string. In case callers
    // wrongly forget to check the return value and look at the string
    // instead, this could catch bugs.
    RAW_LOG(FATAL, "*file_to_open should be NULL");
    return false;
  }
  std::vector<std::string>::const_iterator it;
  it = std::find(allowed_file_names.begin(), allowed_file_names.end(),
                 requested_filename);
  if (it < allowed_file_names.end()) {  // requested_filename was found?
    if (file_to_open)
      *file_to_open = it->c_str();
    return true;
  }
  return false;
}

// We maintain a list of flags that have been reviewed for "sanity" and that
// we're ok to allow in the broker.
// I.e. here is where we wouldn't add O_RESET_FILE_SYSTEM.
bool IsAllowedOpenFlags(int flags) {
  // First, check the access mode
  const int access_mode = flags & O_ACCMODE;
  if (access_mode != O_RDONLY && access_mode != O_WRONLY &&
      access_mode != O_RDWR) {
    return false;
  }

  // We only support a 2-parameters open, so we forbid O_CREAT.
  if (flags & O_CREAT) {
    return false;
  }

  // Some flags affect the behavior of the current process. We don't support
  // them and don't allow them for now.
  if (flags & ForCurrentProcessFlagsMask()) {
    return false;
  }

  // Now check that all the flags are known to us.
  const int creation_and_status_flags = flags & ~O_ACCMODE;

  const int known_flags =
    O_APPEND | O_ASYNC | O_CLOEXEC | O_CREAT | O_DIRECT |
    O_DIRECTORY | O_EXCL | O_LARGEFILE | O_NOATIME | O_NOCTTY |
    O_NOFOLLOW | O_NONBLOCK | O_NDELAY | O_SYNC | O_TRUNC;

  const int unknown_flags = ~known_flags;
  const bool has_unknown_flags = creation_and_status_flags & unknown_flags;
  return !has_unknown_flags;
}

}  // namespace

namespace sandbox {

BrokerProcess::BrokerProcess(const std::vector<std::string>& allowed_r_files,
                             const std::vector<std::string>& allowed_w_files,
                             bool fast_check_in_client,
                             bool quiet_failures_for_tests)
    : initialized_(false),
      is_child_(false),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests),
      broker_pid_(-1),
      allowed_r_files_(allowed_r_files),
      allowed_w_files_(allowed_w_files),
      ipc_socketpair_(-1) {
}

BrokerProcess::~BrokerProcess() {
  if (initialized_ && ipc_socketpair_ != -1) {
    void (HANDLE_EINTR(close(ipc_socketpair_)));
  }
}

bool BrokerProcess::Init(bool (*sandbox_callback)(void)) {
  CHECK(!initialized_);
  int socket_pair[2];
  // Use SOCK_SEQPACKET, because we need to preserve message boundaries
  // but we also want to be notified (recvmsg should return and not block)
  // when the connection has been broken (one of the processes died).
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_pair)) {
    LOG(ERROR) << "Failed to create socketpair";
    return false;
  }

  int child_pid = fork();
  if (child_pid == -1) {
    (void) HANDLE_EINTR(close(socket_pair[0]));
    (void) HANDLE_EINTR(close(socket_pair[1]));
    return false;
  }
  if (child_pid) {
    // We are the parent and we have just forked our broker process.
    (void) HANDLE_EINTR(close(socket_pair[0]));
    // We should only be able to write to the IPC channel. We'll always send
    // a new file descriptor to receive the reply on.
    shutdown(socket_pair[1], SHUT_RD);
    ipc_socketpair_ = socket_pair[1];
    is_child_ = false;
    broker_pid_ = child_pid;
    initialized_ = true;
    return true;
  } else {
    // We are the broker.
    (void) HANDLE_EINTR(close(socket_pair[1]));
    // We should only be able to read from this IPC channel. We will send our
    // replies on a new file descriptor attached to the requests.
    shutdown(socket_pair[0], SHUT_WR);
    ipc_socketpair_ = socket_pair[0];
    is_child_ = true;
    // Enable the sandbox if provided.
    if (sandbox_callback) {
      CHECK(sandbox_callback());
    }
    initialized_ = true;
    for (;;) {
      HandleRequest();
    }
    _exit(1);
  }
  NOTREACHED();
}

// This function needs to be async signal safe.
int BrokerProcess::Open(const char* pathname, int flags) const {
  RAW_CHECK(initialized_);  // async signal safe CHECK().
  if (!pathname)
    return -EFAULT;
  // There is no point in forwarding a request that we know will be denied.
  // Of course, the real security check needs to be on the other side of the
  // IPC.
  if (fast_check_in_client_) {
    if (!GetFileNameIfAllowedAccess(pathname, flags, NULL))
      return -EPERM;
  }

  Pickle write_pickle;
  write_pickle.WriteInt(kCommandOpen);
  write_pickle.WriteString(pathname);
  write_pickle.WriteInt(flags);
  RAW_CHECK(write_pickle.size() <= kMaxMessageLength);

  int returned_fd = -1;
  uint8_t reply_buf[kMaxMessageLength];
  // Send a request (in write_pickle) as well that will include a new
  // temporary socketpair (created internally by SendRecvMsg()).
  // Then read the reply on this new socketpair in reply_buf and put an
  // eventual attached file descriptor in |returned_fd|.
  // TODO(jln): this API needs some rewriting and documentation.
  ssize_t msg_len = UnixDomainSocket::SendRecvMsg(ipc_socketpair_,
                                                  reply_buf,
                                                  sizeof(reply_buf),
                                                  &returned_fd,
                                                  write_pickle);
  if (msg_len <= 0) {
    if (!quiet_failures_for_tests_)
      RAW_LOG(ERROR, "Could not make request to broker process");
    return -ENOMEM;
  }

  Pickle read_pickle(reinterpret_cast<char*>(reply_buf), msg_len);
  PickleIterator iter(read_pickle);
  int return_value = -1;
  // Now deserialize the return value and eventually return the file
  // descriptor.
  if (read_pickle.ReadInt(&iter, &return_value)) {
    if (return_value < 0) {
      RAW_CHECK(returned_fd == -1);
      return return_value;
    } else {
      // We have a real file descriptor to return.
      RAW_CHECK(returned_fd >= 0);
      return returned_fd;
    }
  } else {
    RAW_LOG(ERROR, "Could not read pickle");
    return -1;
  }
}

// Handle a request on the IPC channel ipc_socketpair_.
// A request should have a file descriptor attached on which we will reply and
// that we will then close.
// A request should start with an int that will be used as the command type.
bool BrokerProcess::HandleRequest() const {

  std::vector<int> fds;
  char buf[kMaxMessageLength];
  errno = 0;
  const ssize_t msg_len = UnixDomainSocket::RecvMsg(ipc_socketpair_, buf,
                                                    sizeof(buf), &fds);

  if (msg_len == 0 || (msg_len == -1 && errno == ECONNRESET)) {
    // EOF from our parent, or our parent died, we should die.
    _exit(0);
  }

  // The parent should send exactly one file descriptor, on which we
  // will write the reply.
  if (msg_len < 0 || fds.size() != 1 || fds.at(0) < 0) {
    PLOG(ERROR) << "Error reading message from the client";
    return false;
  }

  const int temporary_ipc = fds.at(0);

  Pickle pickle(buf, msg_len);
  PickleIterator iter(pickle);
  int command_type;
  if (pickle.ReadInt(&iter, &command_type)) {
    bool r = false;
    // Go through all the possible IPC messages.
    switch (command_type) {
      case kCommandOpen:
        // We reply on the file descriptor sent to us via the IPC channel.
        r = HandleOpenRequest(temporary_ipc, pickle, iter);
        (void) HANDLE_EINTR(close(temporary_ipc));
        return r;
      default:
        NOTREACHED();
        return false;
    }
  }

  LOG(ERROR) << "Error parsing IPC request";
  return false;
}

// Handle an open request contained in |read_pickle| and send the reply
// on |reply_ipc|.
bool BrokerProcess::HandleOpenRequest(int reply_ipc,
                                      const Pickle& read_pickle,
                                      PickleIterator iter) const {
  std::string requested_filename;
  int flags = 0;
  if (!read_pickle.ReadString(&iter, &requested_filename) ||
      !read_pickle.ReadInt(&iter, &flags)) {
    return -1;
  }

  Pickle write_pickle;
  std::vector<int> opened_files;

  const char* file_to_open = NULL;
  const bool safe_to_open_file = GetFileNameIfAllowedAccess(
      requested_filename.c_str(), flags, &file_to_open);

  if (safe_to_open_file) {
    CHECK(file_to_open);
    // O_CLOEXEC doesn't hurt (even though we won't execve()), and this
    // property won't be passed to the client.
    // We may want to think about O_NONBLOCK as well.
    // We're doing a 2-parameter open, so we don't support O_CREAT. It doesn't
    // hurt to always pass a third argument though.
    int opened_fd = open(file_to_open, flags | O_CLOEXEC, 0);
    if (opened_fd < 0) {
      write_pickle.WriteInt(-errno);
    } else {
      // Success.
      opened_files.push_back(opened_fd);
      write_pickle.WriteInt(0);
    }
  } else {
    write_pickle.WriteInt(-EPERM);
  }

  CHECK_LE(write_pickle.size(), kMaxMessageLength);
  ssize_t sent = UnixDomainSocket::SendMsg(reply_ipc, write_pickle.data(),
                                           write_pickle.size(), opened_files);

  // Close anything we have opened in this process.
  for (std::vector<int>::iterator it = opened_files.begin();
       it < opened_files.end(); ++it) {
    (void) HANDLE_EINTR(close(*it));
  }

  if (sent <= 0) {
    LOG(ERROR) << "Could not send IPC reply";
    return false;
  }
  return true;
}

// For paranoia, if |file_to_open| is not NULL, we will return the matching
// string from the white list.
// Async signal safe only if |file_to_open| is NULL.
// Even if an attacker managed to fool the string comparison mechanism, we
// would not open an attacker-controlled file name.
// Return true if access should be allowed, false otherwise.
bool BrokerProcess::GetFileNameIfAllowedAccess(const char* requested_filename,
    int requested_flags, const char** file_to_open) const {
  if (!IsAllowedOpenFlags(requested_flags)) {
    return false;
  }
  switch (requested_flags & O_ACCMODE) {
    case O_RDONLY:
      return GetFileNameInWhitelist(allowed_r_files_, requested_filename,
                                    file_to_open);
    case O_WRONLY:
      return GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                    file_to_open);
    case O_RDWR:
    {
      bool allowed_for_read_and_write =
          GetFileNameInWhitelist(allowed_r_files_, requested_filename, NULL) &&
          GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                 file_to_open);
      return allowed_for_read_and_write;
    }
    default:
      return false;
  }
}

}  // namespace sandbox.
