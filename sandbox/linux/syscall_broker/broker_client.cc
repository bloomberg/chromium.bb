// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_client.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#include <utility>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/posix/unix_domain_socket.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"

#if defined(OS_ANDROID) && !defined(MSG_CMSG_CLOEXEC)
#define MSG_CMSG_CLOEXEC 0x40000000
#endif

namespace sandbox {
namespace syscall_broker {

BrokerClient::BrokerClient(const BrokerPermissionList& broker_permission_list,
                           BrokerChannel::EndPoint ipc_channel,
                           const BrokerCommandSet& allowed_command_set,
                           bool fast_check_in_client,
                           bool quiet_failures_for_tests)
    : broker_permission_list_(broker_permission_list),
      ipc_channel_(std::move(ipc_channel)),
      allowed_command_set_(allowed_command_set),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests) {}

BrokerClient::~BrokerClient() {}

int BrokerClient::Access(const char* pathname, int mode) const {
  return PathAndFlagsSyscall(COMMAND_ACCESS, pathname, mode);
}

int BrokerClient::Open(const char* pathname, int flags) const {
  return PathAndFlagsSyscall(COMMAND_OPEN, pathname, flags);
}

int BrokerClient::Stat(const char* pathname, struct stat* sb) {
  return StatFamilySyscall(COMMAND_STAT, pathname, sb, sizeof(*sb));
}

int BrokerClient::Stat64(const char* pathname, struct stat64* sb) {
  return StatFamilySyscall(COMMAND_STAT64, pathname, sb, sizeof(*sb));
}

int BrokerClient::Rename(const char* oldpath, const char* newpath) {
  if (fast_check_in_client_ &&
      !CommandRenameIsSafe(allowed_command_set_, broker_permission_list_,
                           oldpath, newpath, nullptr, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }

  base::Pickle write_pickle;
  write_pickle.WriteInt(COMMAND_RENAME);
  write_pickle.WriteString(oldpath);
  write_pickle.WriteString(newpath);
  RAW_CHECK(write_pickle.size() <= kMaxMessageLength);

  int returned_fd = -1;
  uint8_t reply_buf[kMaxMessageLength];
  ssize_t msg_len = base::UnixDomainSocket::SendRecvMsg(
      ipc_channel_.get(), reply_buf, sizeof(reply_buf), &returned_fd,
      write_pickle);

  if (msg_len <= 0) {
    if (!quiet_failures_for_tests_)
      RAW_LOG(ERROR, "Could not make request to broker process");
    return -ENOMEM;
  }

  base::Pickle read_pickle(reinterpret_cast<char*>(reply_buf), msg_len);
  base::PickleIterator iter(read_pickle);
  int return_value = -1;
  if (!iter.ReadInt(&return_value))
    return -ENOMEM;

  return return_value;
}

int BrokerClient::Readlink(const char* path, char* buf, size_t bufsize) {
  if (fast_check_in_client_ &&
      !CommandReadlinkIsSafe(allowed_command_set_, broker_permission_list_,
                             path, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }

  base::Pickle write_pickle;
  write_pickle.WriteInt(COMMAND_READLINK);
  write_pickle.WriteString(path);
  RAW_CHECK(write_pickle.size() <= kMaxMessageLength);

  int returned_fd = -1;
  uint8_t reply_buf[kMaxMessageLength];
  ssize_t msg_len = base::UnixDomainSocket::SendRecvMsg(
      ipc_channel_.get(), reply_buf, sizeof(reply_buf), &returned_fd,
      write_pickle);

  if (msg_len <= 0) {
    if (!quiet_failures_for_tests_)
      RAW_LOG(ERROR, "Could not make request to broker process");
    return -ENOMEM;
  }

  base::Pickle read_pickle(reinterpret_cast<char*>(reply_buf), msg_len);
  base::PickleIterator iter(read_pickle);
  int return_value = -1;
  int return_length = 0;
  const char* return_data = nullptr;
  if (!iter.ReadInt(&return_value))
    return -ENOMEM;
  if (return_value < 0)
    return return_value;
  if (!iter.ReadData(&return_data, &return_length))
    return -ENOMEM;
  if (return_length < 0)
    return -ENOMEM;
  if (static_cast<size_t>(return_length) > bufsize)
    return -ENAMETOOLONG;
  memcpy(buf, return_data, return_length);
  return return_value;
}

// Make a remote system call over IPC for syscalls that take a path and flags
// as arguments, currently open() and access().
// Will return -errno like a real system call.
// This function needs to be async signal safe.
int BrokerClient::PathAndFlagsSyscall(BrokerCommand syscall_type,
                                      const char* pathname,
                                      int flags) const {
  int recvmsg_flags = 0;
  RAW_CHECK(syscall_type == COMMAND_OPEN || syscall_type == COMMAND_ACCESS);
  if (!pathname)
    return -EFAULT;

  // For this "remote system call" to work, we need to handle any flag that
  // cannot be sent over a Unix socket in a special way.
  // See the comments around kCurrentProcessOpenFlagsMask.
  if (syscall_type == COMMAND_OPEN && (flags & kCurrentProcessOpenFlagsMask)) {
    // This implementation only knows about O_CLOEXEC, someone needs to look at
    // this code if other flags are added.
    RAW_CHECK(kCurrentProcessOpenFlagsMask == O_CLOEXEC);
    recvmsg_flags |= MSG_CMSG_CLOEXEC;
    flags &= ~O_CLOEXEC;
  }

  // There is no point in forwarding a request that we know will be denied.
  // Of course, the real security check needs to be on the other side of the
  // IPC.
  if (fast_check_in_client_) {
    if (syscall_type == COMMAND_OPEN &&
        !CommandOpenIsSafe(allowed_command_set_, broker_permission_list_,
                           pathname, flags, NULL /* file_to_open */,
                           NULL /* unlink_after_open */)) {
      return -broker_permission_list_.denied_errno();
    }
    if (syscall_type == COMMAND_ACCESS &&
        !CommandAccessIsSafe(allowed_command_set_, broker_permission_list_,
                             pathname, flags, NULL)) {
      return -broker_permission_list_.denied_errno();
    }
  }

  base::Pickle write_pickle;
  write_pickle.WriteInt(syscall_type);
  write_pickle.WriteString(pathname);
  write_pickle.WriteInt(flags);
  RAW_CHECK(write_pickle.size() <= kMaxMessageLength);

  int returned_fd = -1;
  uint8_t reply_buf[kMaxMessageLength];

  // Send a request (in write_pickle) as well that will include a new
  // temporary socketpair (created internally by SendRecvMsg()).
  // Then read the reply on this new socketpair in reply_buf and put an
  // eventual attached file descriptor in |returned_fd|.
  ssize_t msg_len = base::UnixDomainSocket::SendRecvMsgWithFlags(
      ipc_channel_.get(), reply_buf, sizeof(reply_buf), recvmsg_flags,
      &returned_fd, write_pickle);
  if (msg_len <= 0) {
    if (!quiet_failures_for_tests_)
      RAW_LOG(ERROR, "Could not make request to broker process");
    return -ENOMEM;
  }

  base::Pickle read_pickle(reinterpret_cast<char*>(reply_buf), msg_len);
  base::PickleIterator iter(read_pickle);
  int return_value = -1;
  // Now deserialize the return value and eventually return the file
  // descriptor.
  if (iter.ReadInt(&return_value)) {
    switch (syscall_type) {
      case COMMAND_ACCESS:
        // We should never have a fd to return.
        RAW_CHECK(returned_fd == -1);
        return return_value;
      case COMMAND_OPEN:
        if (return_value < 0) {
          RAW_CHECK(returned_fd == -1);
          return return_value;
        } else {
          // We have a real file descriptor to return.
          RAW_CHECK(returned_fd >= 0);
          return returned_fd;
        }
      default:
        RAW_LOG(ERROR, "Unsupported command");
        return -ENOSYS;
    }
  } else {
    RAW_LOG(ERROR, "Could not read pickle");
    NOTREACHED();
    return -ENOMEM;
  }
}

int BrokerClient::StatFamilySyscall(BrokerCommand syscall_type,
                                    const char* pathname,
                                    void* result_ptr,
                                    size_t expected_result_size) const {
  if (fast_check_in_client_ &&
      !CommandStatIsSafe(allowed_command_set_, broker_permission_list_,
                         pathname, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }

  base::Pickle write_pickle;
  write_pickle.WriteInt(syscall_type);
  write_pickle.WriteString(pathname);
  RAW_CHECK(write_pickle.size() <= kMaxMessageLength);

  int returned_fd = -1;
  uint8_t reply_buf[kMaxMessageLength];
  ssize_t msg_len = base::UnixDomainSocket::SendRecvMsg(
      ipc_channel_.get(), reply_buf, sizeof(reply_buf), &returned_fd,
      write_pickle);

  if (msg_len <= 0) {
    if (!quiet_failures_for_tests_)
      RAW_LOG(ERROR, "Could not make request to broker process");
    return -ENOMEM;
  }

  base::Pickle read_pickle(reinterpret_cast<char*>(reply_buf), msg_len);
  base::PickleIterator iter(read_pickle);
  int return_value = -1;
  int return_length = 0;
  const char* return_data = nullptr;
  if (!iter.ReadInt(&return_value))
    return -ENOMEM;
  if (return_value < 0)
    return return_value;
  if (!iter.ReadData(&return_data, &return_length))
    return -ENOMEM;
  if (static_cast<size_t>(return_length) != expected_result_size)
    return -ENOMEM;
  memcpy(result_ptr, return_data, expected_result_size);
  return return_value;
}

}  // namespace syscall_broker
}  // namespace sandbox
