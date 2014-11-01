// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_POLICY_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_POLICY_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace sandbox {
namespace syscall_broker {

// BrokerPolicy allows to define the security policy enforced by a
// BrokerHost. The BrokerHost will evaluate requests sent over its
// IPC channel according to the BrokerPolicy.
// Some of the methods of this class can be used in an async-signal safe
// way.
class BrokerPolicy {
 public:
  // |denied_errno| is the error code returned when IPC requests for system
  // calls such as open() or access() are denied because a file is not in the
  // whitelist. EACCESS would be a typical value.
  // |allowed_r_files| and |allowed_w_files| are white lists of files that
  // should be allowed for opening, respectively for reading and writing.
  // A file available read-write should be listed in both.
  BrokerPolicy(int denied_errno,
               const std::vector<std::string>& allowed_r_files,
               const std::vector<std::string>& allowed_w_files_);
  ~BrokerPolicy();

  // Check if calling access() should be allowed on |requested_filename| with
  // mode |requested_mode|.
  // Note: access() being a system call to check permissions, this can get a bit
  // confusing. We're checking if calling access() should even be allowed with
  // the same policy we would use for open().
  // If |file_to_access| is not NULL, we will return the matching pointer from
  // the whitelist. For paranoia a caller should then use |file_to_access|. See
  // GetFileNameIfAllowedToOpen() for more explanation.
  // return true if calling access() on this file should be allowed, false
  // otherwise.
  // Async signal safe if and only if |file_to_access| is NULL.
  bool GetFileNameIfAllowedToAccess(const char* requested_filename,
                                    int requested_mode,
                                    const char** file_to_access) const;

  // Check if |requested_filename| can be opened with flags |requested_flags|.
  // If |file_to_open| is not NULL, we will return the matching pointer from the
  // whitelist. For paranoia, a caller should then use |file_to_open| rather
  // than |requested_filename|, so that it never attempts to open an
  // attacker-controlled file name, even if an attacker managed to fool the
  // string comparison mechanism.
  // Return true if opening should be allowed, false otherwise.
  // Async signal safe if and only if |file_to_open| is NULL.
  bool GetFileNameIfAllowedToOpen(const char* requested_filename,
                                  int requested_flags,
                                  const char** file_to_open) const;
  int denied_errno() const { return denied_errno_; }

 private:
  const int denied_errno_;
  const std::vector<std::string> allowed_r_files_;
  const std::vector<std::string> allowed_w_files_;
  DISALLOW_COPY_AND_ASSIGN(BrokerPolicy);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_POLICY_H_
