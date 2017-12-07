// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"

namespace sandbox {
namespace syscall_broker {

bool CommandAccessIsSafe(const BrokerCommandSet& command_set,
                         const BrokerPermissionList& policy,
                         const char* requested_filename,
                         int requested_mode,
                         const char** filename_to_use) {
  return command_set.test(COMMAND_ACCESS) &&
         policy.GetFileNameIfAllowedToAccess(requested_filename, requested_mode,
                                             filename_to_use);
}

bool CommandOpenIsSafe(const BrokerCommandSet& command_set,
                       const BrokerPermissionList& policy,
                       const char* requested_filename,
                       int requested_flags,
                       const char** filename_to_use,
                       bool* unlink_after_open) {
  return command_set.test(COMMAND_OPEN) &&
         policy.GetFileNameIfAllowedToOpen(requested_filename, requested_flags,
                                           filename_to_use, unlink_after_open);
}

bool CommandReadlinkIsSafe(const BrokerCommandSet& command_set,
                           const BrokerPermissionList& policy,
                           const char* requested_filename,
                           const char** filename_to_use) {
  return command_set.test(COMMAND_READLINK) &&
         policy.GetFileNameIfAllowedToOpen(requested_filename, O_RDONLY,
                                           filename_to_use, nullptr);
}

bool CommandRenameIsSafe(const BrokerCommandSet& command_set,
                         const BrokerPermissionList& policy,
                         const char* old_filename,
                         const char* new_filename,
                         const char** old_filename_to_use,
                         const char** new_filename_to_use) {
  return command_set.test(COMMAND_RENAME) &&
         policy.GetFileNameIfAllowedToOpen(old_filename, O_RDWR,
                                           old_filename_to_use, nullptr) &&
         policy.GetFileNameIfAllowedToOpen(new_filename, O_RDWR,
                                           new_filename_to_use, nullptr);
}

bool CommandStatIsSafe(const BrokerCommandSet& command_set,
                       const BrokerPermissionList& policy,
                       const char* requested_filename,
                       const char** filename_to_use) {
  return command_set.test(COMMAND_STAT) &&
         policy.GetFileNameIfAllowedToAccess(requested_filename, F_OK,
                                             filename_to_use);
}

}  // namespace syscall_broker
}  // namespace sandbox
