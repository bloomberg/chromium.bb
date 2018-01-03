// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_host.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
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
  return syscall(__NR_openat, AT_FDCWD, pathname, flags, mode);
}

// Perform access(2) on |requested_filename| with mode |mode| if allowed by our
// permission_list. Write the syscall return value (-errno) to |write_pickle|.
void AccessFileForIPC(const BrokerCommandSet& allowed_command_set,
                      const BrokerPermissionList& permission_list,
                      const std::string& requested_filename,
                      int mode,
                      base::Pickle* write_pickle) {
  const char* file_to_access = NULL;
  if (!CommandAccessIsSafe(allowed_command_set, permission_list,
                           requested_filename.c_str(), mode, &file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }

  CHECK(file_to_access);
  if (access(file_to_access, mode) < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }

  write_pickle->WriteInt(0);
}

void MkdirFileForIPC(const BrokerCommandSet& allowed_command_set,
                     const BrokerPermissionList& permission_list,
                     const std::string& filename,
                     int mode,
                     base::Pickle* write_pickle) {
  const char* file_to_access = nullptr;
  if (!CommandMkdirIsSafe(allowed_command_set, permission_list,
                          filename.c_str(), &file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }
  if (mkdir(file_to_access, mode) < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }
  write_pickle->WriteInt(0);
}

// Open |requested_filename| with |flags| if allowed by our permission list.
// Write the syscall return value (-errno) to |write_pickle| and append
// a file descriptor to |opened_files| if relevant.
void OpenFileForIPC(const BrokerCommandSet& allowed_command_set,
                    const BrokerPermissionList& permission_list,
                    const std::string& requested_filename,
                    int flags,
                    base::Pickle* write_pickle,
                    std::vector<int>* opened_files) {
  const char* file_to_open = NULL;
  bool unlink_after_open = false;
  if (!CommandOpenIsSafe(allowed_command_set, permission_list,
                         requested_filename.c_str(), flags, &file_to_open,
                         &unlink_after_open)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }

  CHECK(file_to_open);
  int opened_fd = sys_open(file_to_open, flags);
  if (opened_fd < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }

  if (unlink_after_open)
    unlink(file_to_open);

  opened_files->push_back(opened_fd);
  write_pickle->WriteInt(0);
}

// Perform rename(2) on |old_filename| to |new_filename| and marshal the
// result to |write_pickle|.
void RenameFileForIPC(const BrokerCommandSet& allowed_command_set,
                      const BrokerPermissionList& permission_list,
                      const std::string& old_filename,
                      const std::string& new_filename,
                      base::Pickle* write_pickle) {
  const char* old_file_to_access = nullptr;
  const char* new_file_to_access = nullptr;
  if (!CommandRenameIsSafe(allowed_command_set, permission_list,
                           old_filename.c_str(), new_filename.c_str(),
                           &old_file_to_access, &new_file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }
  if (rename(old_file_to_access, new_file_to_access) < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }
  write_pickle->WriteInt(0);
}

// Perform readlink(2) on |filename| using a buffer of MAX_PATH bytes.
void ReadlinkFileForIPC(const BrokerCommandSet& allowed_command_set,
                        const BrokerPermissionList& permission_list,
                        const std::string& filename,
                        base::Pickle* write_pickle) {
  const char* file_to_access = nullptr;
  if (!CommandReadlinkIsSafe(allowed_command_set, permission_list,
                             filename.c_str(), &file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }
  char buf[PATH_MAX];
  ssize_t result = readlink(file_to_access, buf, sizeof(buf));
  if (result < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }
  write_pickle->WriteInt(result);
  write_pickle->WriteData(buf, result);
}

void RmdirFileForIPC(const BrokerCommandSet& allowed_command_set,
                     const BrokerPermissionList& permission_list,
                     const std::string& requested_filename,
                     base::Pickle* write_pickle) {
  const char* file_to_access = nullptr;
  if (!CommandRmdirIsSafe(allowed_command_set, permission_list,
                          requested_filename.c_str(), &file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }
  if (rmdir(file_to_access) < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }
  write_pickle->WriteInt(0);
}

// Perform stat(2) on |requested_filename| and marshal the result to
// |write_pickle|.
void StatFileForIPC(const BrokerCommandSet& allowed_command_set,
                    const BrokerPermissionList& permission_list,
                    BrokerCommand command_type,
                    const std::string& requested_filename,
                    base::Pickle* write_pickle) {
  const char* file_to_access = nullptr;
  if (!CommandStatIsSafe(allowed_command_set, permission_list,
                         requested_filename.c_str(), &file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
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
    DCHECK(command_type == COMMAND_STAT64);
#if defined(__ANDROID_API__) && __ANDROID_API__ < 21
    // stat64 is not defined for older Android API versions in newer NDK
    // versions.
    write_pickle->WriteInt(-ENOSYS);
    return;
#else
    struct stat64 sb;
    if (stat64(file_to_access, &sb) < 0) {
      write_pickle->WriteInt(-errno);
      return;
    }
    write_pickle->WriteInt(0);
    write_pickle->WriteData(reinterpret_cast<char*>(&sb), sizeof(sb));
#endif  // defined(__ANDROID_API__) && __ANDROID_API__ < 21
  }
}

void UnlinkFileForIPC(const BrokerCommandSet& allowed_command_set,
                      const BrokerPermissionList& permission_list,
                      const std::string& requested_filename,
                      base::Pickle* write_pickle) {
  const char* file_to_access = nullptr;
  if (!CommandUnlinkIsSafe(allowed_command_set, permission_list,
                           requested_filename.c_str(), &file_to_access)) {
    write_pickle->WriteInt(-permission_list.denied_errno());
    return;
  }
  if (unlink(file_to_access) < 0) {
    write_pickle->WriteInt(-errno);
    return;
  }
  write_pickle->WriteInt(0);
}

// Handle a |command_type| request contained in |iter| and write the reply
// to |write_pickle|, adding any files opened to |opened_files|.
bool HandleRemoteCommand(const BrokerCommandSet& allowed_command_set,
                         const BrokerPermissionList& permission_list,
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
      AccessFileForIPC(allowed_command_set, permission_list, requested_filename,
                       flags, write_pickle);
      break;
    }
    case COMMAND_MKDIR: {
      std::string requested_filename;
      int mode = 0;
      if (!iter.ReadString(&requested_filename) || !iter.ReadInt(&mode))
        return false;
      MkdirFileForIPC(allowed_command_set, permission_list, requested_filename,
                      mode, write_pickle);
      break;
    }
    case COMMAND_OPEN: {
      std::string requested_filename;
      int flags = 0;
      if (!iter.ReadString(&requested_filename) || !iter.ReadInt(&flags))
        return false;
      OpenFileForIPC(allowed_command_set, permission_list, requested_filename,
                     flags, write_pickle, opened_files);
      break;
    }
    case COMMAND_READLINK: {
      std::string filename;
      if (!iter.ReadString(&filename))
        return false;
      ReadlinkFileForIPC(allowed_command_set, permission_list, filename,
                         write_pickle);
      break;
    }
    case COMMAND_RENAME: {
      std::string old_filename;
      std::string new_filename;
      if (!iter.ReadString(&old_filename) || !iter.ReadString(&new_filename))
        return false;
      RenameFileForIPC(allowed_command_set, permission_list, old_filename,
                       new_filename, write_pickle);
      break;
    }
    case COMMAND_RMDIR: {
      std::string requested_filename;
      if (!iter.ReadString(&requested_filename))
        return false;
      RmdirFileForIPC(allowed_command_set, permission_list, requested_filename,
                      write_pickle);
      break;
    }
    case COMMAND_STAT:
    case COMMAND_STAT64: {
      std::string requested_filename;
      if (!iter.ReadString(&requested_filename))
        return false;
      StatFileForIPC(allowed_command_set, permission_list,
                     static_cast<BrokerCommand>(command_type),
                     requested_filename, write_pickle);
      break;
    }
    case COMMAND_UNLINK: {
      std::string requested_filename;
      if (!iter.ReadString(&requested_filename))
        return false;
      UnlinkFileForIPC(allowed_command_set, permission_list, requested_filename,
                       write_pickle);
      break;
    }
    default:
      LOG(ERROR) << "Invalid IPC command";
      return false;
  }
  return true;
}

}  // namespace

BrokerHost::BrokerHost(const BrokerPermissionList& broker_permission_list,
                       const BrokerCommandSet& allowed_command_set,
                       BrokerChannel::EndPoint ipc_channel)
    : broker_permission_list_(broker_permission_list),
      allowed_command_set_(allowed_command_set),
      ipc_channel_(std::move(ipc_channel)) {}

BrokerHost::~BrokerHost() {}

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
      HandleRemoteCommand(allowed_command_set_, broker_permission_list_, iter,
                          &write_pickle, &opened_files);

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
