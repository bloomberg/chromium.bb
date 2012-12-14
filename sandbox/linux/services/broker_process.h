// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_
#define SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/process.h"

namespace sandbox {

// Create a new "broker" process to which we can send requests via an IPC
// channel.
// This is a low level IPC mechanism that is suitable to be called from a
// signal handler.
// A process would typically create a broker process before entering
// sandboxing.
// 1. BrokerProcess open_broker(read_whitelist, write_whitelist);
// 2. CHECK(open_broker.Init(NULL));
// 3. Enable sandbox.
// 4. Use open_broker.Open() to open files.
class BrokerProcess {
 public:
  // |allowed_file_names| is a white list of files that can be opened later via
  // the Open() API.
  // |fast_check_in_client| and |quiet_failures_for_tests| are reserved for
  // unit tests, don't use it.
  explicit BrokerProcess(const std::vector<std::string>& allowed_r_files_,
                         const std::vector<std::string>& allowed_w_files_,
                         bool fast_check_in_client = true,
                         bool quiet_failures_for_tests = false);
  ~BrokerProcess();
  // Will initialize the broker process. There should be no threads at this
  // point, since we need to fork().
  // sandbox_callback is a function that should be called to enable the
  // sandbox in the broker.
  bool Init(bool (*sandbox_callback)(void));

  // Can be used in place of open(). Will be async signal safe.
  // The implementation only supports certain white listed flags and will
  // return -EPERM on other flags.
  // It's similar to the open() system call and will return -errno on errors.
  int Open(const char* pathname, int flags) const;

  int broker_pid() const { return broker_pid_; }

 private:
  bool HandleRequest() const;
  bool HandleOpenRequest(int reply_ipc, const Pickle& read_pickle,
                         PickleIterator iter) const;
  bool GetFileNameIfAllowedAccess(const char*, int, const char**) const;
  bool initialized_;  // Whether we've been through Init() yet.
  bool is_child_;  // Whether we're the child (broker process).
  bool fast_check_in_client_;  // Whether to forward a request that we know
                               // will be denied to the broker.
  bool quiet_failures_for_tests_;  // Disable certain error message when
                                   // testing for failures.
  pid_t broker_pid_;  // The PID of the broker (child).
  const std::vector<std::string> allowed_r_files_;  // Files allowed for read.
  const std::vector<std::string> allowed_w_files_;  // Files allowed for write.
  int ipc_socketpair_;  // Our communication channel to parent or child.
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrokerProcess);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_
