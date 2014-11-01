// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_host.h"

namespace sandbox {

BrokerProcess::BrokerProcess(int denied_errno,
                             const std::vector<std::string>& allowed_r_files,
                             const std::vector<std::string>& allowed_w_files,
                             bool fast_check_in_client,
                             bool quiet_failures_for_tests)
    : initialized_(false),
      is_child_(false),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests),
      broker_pid_(-1),
      policy_(denied_errno, allowed_r_files, allowed_w_files),
      ipc_socketpair_(-1) {
}

BrokerProcess::~BrokerProcess() {
  if (initialized_ && ipc_socketpair_ != -1) {
    // Closing the socket should be enough to notify the child to die,
    // unless it has been duplicated.
    PCHECK(0 == IGNORE_EINTR(close(ipc_socketpair_)));
    PCHECK(0 == kill(broker_pid_, SIGKILL));
    siginfo_t process_info;
    // Reap the child.
    int ret = HANDLE_EINTR(waitid(P_PID, broker_pid_, &process_info, WEXITED));
    PCHECK(0 == ret);
  }
}

bool BrokerProcess::Init(
    const base::Callback<bool(void)>& broker_process_init_callback) {
  CHECK(!initialized_);
  int socket_pair[2];
  // Use SOCK_SEQPACKET, because we need to preserve message boundaries
  // but we also want to be notified (recvmsg should return and not block)
  // when the connection has been broken (one of the processes died).
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_pair)) {
    LOG(ERROR) << "Failed to create socketpair";
    return false;
  }

#if !defined(THREAD_SANITIZER)
  DCHECK_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
#endif
  int child_pid = fork();
  if (child_pid == -1) {
    close(socket_pair[0]);
    close(socket_pair[1]);
    return false;
  }
  if (child_pid) {
    // We are the parent and we have just forked our broker process.
    close(socket_pair[0]);
    // We should only be able to write to the IPC channel. We'll always send
    // a new file descriptor to receive the reply on.
    shutdown(socket_pair[1], SHUT_RD);
    ipc_socketpair_ = socket_pair[1];
    is_child_ = false;
    broker_pid_ = child_pid;
    broker_client_.reset(
        new syscall_broker::BrokerClient(policy_,
                                         ipc_socketpair_,
                                         fast_check_in_client_,
                                         quiet_failures_for_tests_));
    initialized_ = true;
    return true;
  } else {
    // We are the broker.
    close(socket_pair[1]);
    // We should only be able to read from this IPC channel. We will send our
    // replies on a new file descriptor attached to the requests.
    shutdown(socket_pair[0], SHUT_WR);
    ipc_socketpair_ = socket_pair[0];
    is_child_ = true;
    CHECK(broker_process_init_callback.Run());
    syscall_broker::BrokerHost broker_host(policy_, ipc_socketpair_);
    initialized_ = true;
    for (;;) {
      broker_host.HandleRequest();
    }
    _exit(1);
  }
  NOTREACHED();
}

int BrokerProcess::Access(const char* pathname, int mode) const {
  RAW_CHECK(initialized_);
  return broker_client_->Access(pathname, mode);
}

int BrokerProcess::Open(const char* pathname, int flags) const {
  RAW_CHECK(initialized_);
  return broker_client_->Open(pathname, flags);
}

}  // namespace sandbox.
