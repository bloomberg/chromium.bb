// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_host.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/third_party/valgrind/valgrind.h"
#include "sandbox/linux/syscall_broker/broker_common.h"
#include "sandbox/linux/syscall_broker/broker_policy.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {

namespace syscall_broker {

namespace {

// A little open(2) wrapper to handle some oddities for us. In the general case
// make a direct system call since we want to keep in control of the broker
// process' system calls profile to be able to loosely sandbox it.
int sys_open(const char* pathname, int flags) {
  // Hardcode mode to rw------- when creating files.
  int mode = (flags & O_CREAT) ? 0600 : 0;
  if (RunningOnValgrind()) {
    // Valgrind does not support AT_FDCWD, just use libc's open() in this case.
    return open(pathname, flags, mode);
  }
  return syscall(__NR_openat, AT_FDCWD, pathname, flags, mode);
}

// Open |requested_filename| with |flags| if allowed by our policy.
// Write the syscall return value (-errno) to |write_pickle| and append
// a file descriptor to |opened_files| if relevant.
void OpenFileForIPC(const BrokerPolicy& policy,
                    const std::string& requested_filename,
                    int flags,
                    base::Pickle* write_pickle,
                    std::vector<int>* opened_files) {
  DCHECK(write_pickle);
  DCHECK(opened_files);
  const char* file_to_open = NULL;
  bool unlink_after_open = false;
  const bool safe_to_open_file = policy.GetFileNameIfAllowedToOpen(
      requested_filename.c_str(), flags, &file_to_open, &unlink_after_open);

  if (safe_to_open_file) {
    CHECK(file_to_open);
    int opened_fd = sys_open(file_to_open, flags);
    if (opened_fd < 0) {
      write_pickle->WriteInt(-errno);
    } else {
      // Success.
      if (unlink_after_open) {
        unlink(file_to_open);
      }
      opened_files->push_back(opened_fd);
      write_pickle->WriteInt(0);
    }
  } else {
    write_pickle->WriteInt(-policy.denied_errno());
  }
}

// Perform access(2) on |requested_filename| with mode |mode| if allowed by our
// policy. Write the syscall return value (-errno) to |write_pickle|.
void AccessFileForIPC(const BrokerPolicy& policy,
                      const std::string& requested_filename,
                      int mode,
                      base::Pickle* write_pickle) {
  DCHECK(write_pickle);
  const char* file_to_access = NULL;
  const bool safe_to_access_file = policy.GetFileNameIfAllowedToAccess(
      requested_filename.c_str(), mode, &file_to_access);

  if (safe_to_access_file) {
    CHECK(file_to_access);
    int access_ret = access(file_to_access, mode);
    int access_errno = errno;
    if (!access_ret)
      write_pickle->WriteInt(0);
    else
      write_pickle->WriteInt(-access_errno);
  } else {
    write_pickle->WriteInt(-policy.denied_errno());
  }
}

// Perform stat(2) on |requested_filename| and marshal the result to
// |write_pickle|.
void StatFileForIPC(const BrokerPolicy& policy,
                    IPCCommand command_type,
                    const std::string& requested_filename,
                    base::Pickle* write_pickle) {
  DCHECK(write_pickle);
  DCHECK(command_type == COMMAND_STAT || command_type == COMMAND_STAT64);
  const char* file_to_access = nullptr;
  if (!policy.GetFileNameIfAllowedToAccess(requested_filename.c_str(), F_OK,
                                           &file_to_access)) {
    write_pickle->WriteInt(-policy.denied_errno());
    return;
  }
  if (command_type == COMMAND_STAT) {
    struct stat sb;
    if (stat(file_to_access, &sb) < 0) {
      write_pickle->WriteInt(-errno);
      return;
    }
    write_pickle->WriteInt(0);
    write_pickle->WriteData(reinterpret_cast<char*>(&sb), sizeof(sb));
  } else {
    struct stat64 sb;
    if (stat64(file_to_access, &sb) < 0) {
      write_pickle->WriteInt(-errno);
      return;
    }
    write_pickle->WriteInt(0);
    write_pickle->WriteData(reinterpret_cast<char*>(&sb), sizeof(sb));
  }
}

// Perform rename(2) on |old_filename| to |new_filename| and marshal the
// result to |write_pickle|.
void RenameFileForIPC(const BrokerPolicy& policy,
                      const std::string& old_filename,
                      const std::string& new_filename,
                      base::Pickle* write_pickle) {
  DCHECK(write_pickle);
  bool ignore;
  const char* old_file_to_access = nullptr;
  const char* new_file_to_access = nullptr;
  if (!policy.GetFileNameIfAllowedToOpen(old_filename.c_str(), O_RDWR,
                                         &old_file_to_access, &ignore) ||
      !policy.GetFileNameIfAllowedToOpen(new_filename.c_str(), O_RDWR,
                                         &new_file_to_access, &ignore)) {
    write_pickle->WriteInt(-policy.denied_errno());
    return;
  }
  if (rename(old_file_to_access, new_file_to_access) < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }
  write_pickle->WriteInt(0);
}

// Handle a |command_type| request contained in |iter| and write the reply
// to |write_pickle|, adding any files opened to |opened_files|.
bool HandleRemoteCommand(const BrokerPolicy& policy,
                         base::PickleIterator iter,
                         base::Pickle* write_pickle,
                         std::vector<int>* opened_files) {
  int command_type;
  if (!iter.ReadInt(&command_type))
    return false;

  switch (command_type) {
    case COMMAND_ACCESS: {
      std::string requested_filename;
      int flags = 0;
      if (!iter.ReadString(&requested_filename) || !iter.ReadInt(&flags))
        return false;
      AccessFileForIPC(policy, requested_filename, flags, write_pickle);
      break;
    }
    case COMMAND_OPEN: {
      std::string requested_filename;
      int flags = 0;
      if (!iter.ReadString(&requested_filename) || !iter.ReadInt(&flags))
        return false;
      OpenFileForIPC(policy, requested_filename, flags, write_pickle,
                     opened_files);
      break;
    }
    case COMMAND_STAT:
    case COMMAND_STAT64: {
      std::string requested_filename;
      if (!iter.ReadString(&requested_filename))
        return false;
      StatFileForIPC(policy, static_cast<IPCCommand>(command_type),
                     requested_filename, write_pickle);
      break;
    }
    case COMMAND_RENAME: {
      std::string old_filename;
      std::string new_filename;
      if (!iter.ReadString(&old_filename) || !iter.ReadString(&new_filename))
        return false;
      RenameFileForIPC(policy, old_filename, new_filename, write_pickle);
      break;
    }
    default:
      LOG(ERROR) << "Invalid IPC command";
      return false;
  }
  return true;
}

}  // namespace

BrokerHost::BrokerHost(const BrokerPolicy& broker_policy,
                       BrokerChannel::EndPoint ipc_channel)
    : broker_policy_(broker_policy), ipc_channel_(std::move(ipc_channel)) {}

BrokerHost::~BrokerHost() {
}

// Handle a request on the IPC channel ipc_channel_.
// A request should have a file descriptor attached on which we will reply and
// that we will then close.
// A request should start with an int that will be used as the command type.
BrokerHost::RequestStatus BrokerHost::HandleRequest() const {
  std::vector<base::ScopedFD> fds;
  char buf[kMaxMessageLength];
  errno = 0;
  const ssize_t msg_len = base::UnixDomainSocket::RecvMsg(
      ipc_channel_.get(), buf, sizeof(buf), &fds);

  if (msg_len == 0 || (msg_len == -1 && errno == ECONNRESET)) {
    // EOF from the client, or the client died, we should die.
    return RequestStatus::LOST_CLIENT;
  }

  // The client should send exactly one file descriptor, on which we
  // will write the reply.
  if (msg_len < 0 || fds.size() != 1 || fds[0].get() < 0) {
    PLOG(ERROR) << "Error reading message from the client";
    return RequestStatus::FAILURE;
  }

  base::ScopedFD temporary_ipc(std::move(fds[0]));

  base::Pickle pickle(buf, msg_len);
  base::PickleIterator iter(pickle);
  base::Pickle write_pickle;
  std::vector<int> opened_files;
  bool result =
      HandleRemoteCommand(broker_policy_, iter, &write_pickle, &opened_files);

  if (result) {
    CHECK_LE(write_pickle.size(), kMaxMessageLength);
    ssize_t sent = base::UnixDomainSocket::SendMsg(
        temporary_ipc.get(), write_pickle.data(), write_pickle.size(),
        opened_files);
    if (sent <= 0) {
      LOG(ERROR) << "Could not send IPC reply";
      result = false;
    }
  }

  // Close anything we have opened in this process.
  for (int fd : opened_files) {
    int ret = IGNORE_EINTR(close(fd));
    DCHECK(!ret) << "Could not close file descriptor";
  }

  return result ? RequestStatus::SUCCESS : RequestStatus::FAILURE;
}

}  // namespace syscall_broker

}  // namespace sandbox
