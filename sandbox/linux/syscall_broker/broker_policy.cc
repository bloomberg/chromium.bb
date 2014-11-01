// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_policy.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "sandbox/linux/syscall_broker/broker_common.h"

namespace sandbox {
namespace syscall_broker {

namespace {

// We maintain a list of flags that have been reviewed for "sanity" and that
// we're ok to allow in the broker.
// I.e. here is where we wouldn't add O_RESET_FILE_SYSTEM.
bool IsAllowedOpenFlags(int flags) {
  // First, check the access mode.
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
  if (flags & kCurrentProcessOpenFlagsMask)
    return false;

  // Now check that all the flags are known to us.
  const int creation_and_status_flags = flags & ~O_ACCMODE;

  const int known_flags = O_APPEND | O_ASYNC | O_CLOEXEC | O_CREAT | O_DIRECT |
                          O_DIRECTORY | O_EXCL | O_LARGEFILE | O_NOATIME |
                          O_NOCTTY | O_NOFOLLOW | O_NONBLOCK | O_NDELAY |
                          O_SYNC | O_TRUNC;

  const int unknown_flags = ~known_flags;
  const bool has_unknown_flags = creation_and_status_flags & unknown_flags;
  return !has_unknown_flags;
}

// Needs to be async signal safe if |file_to_open| is NULL.
// TODO(jln): assert signal safety.
bool GetFileNameInWhitelist(const std::vector<std::string>& allowed_file_names,
                            const char* requested_filename,
                            const char** file_to_open) {
  if (file_to_open && *file_to_open) {
    // Make sure that callers never pass a non-empty string. In case callers
    // wrongly forget to check the return value and look at the string
    // instead, this could catch bugs.
    RAW_LOG(FATAL, "*file_to_open should be NULL");
    return false;
  }

  // Look for |requested_filename| in |allowed_file_names|.
  // We don't use ::find() because it takes a std::string and
  // the conversion allocates memory.
  for (const auto& allowed_file_name : allowed_file_names) {
    if (strcmp(requested_filename, allowed_file_name.c_str()) == 0) {
      if (file_to_open)
        *file_to_open = allowed_file_name.c_str();
      return true;
    }
  }
  return false;
}

}  // namespace

BrokerPolicy::BrokerPolicy(int denied_errno,
                           const std::vector<std::string>& allowed_r_files,
                           const std::vector<std::string>& allowed_w_files)
    : denied_errno_(denied_errno),
      allowed_r_files_(allowed_r_files),
      allowed_w_files_(allowed_w_files) {
}

BrokerPolicy::~BrokerPolicy() {
}

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
bool BrokerPolicy::GetFileNameIfAllowedToAccess(
    const char* requested_filename,
    int requested_mode,
    const char** file_to_access) const {
  // First, check if |requested_mode| is existence, ability to read or ability
  // to write. We do not support X_OK.
  if (requested_mode != F_OK && requested_mode & ~(R_OK | W_OK)) {
    return false;
  }
  switch (requested_mode) {
    case F_OK:
      // We allow to check for file existence if we can either read or write.
      return GetFileNameInWhitelist(
                 allowed_r_files_, requested_filename, file_to_access) ||
             GetFileNameInWhitelist(
                 allowed_w_files_, requested_filename, file_to_access);
    case R_OK:
      return GetFileNameInWhitelist(
          allowed_r_files_, requested_filename, file_to_access);
    case W_OK:
      return GetFileNameInWhitelist(
          allowed_w_files_, requested_filename, file_to_access);
    case R_OK | W_OK: {
      bool allowed_for_read_and_write =
          GetFileNameInWhitelist(allowed_r_files_, requested_filename, NULL) &&
          GetFileNameInWhitelist(
              allowed_w_files_, requested_filename, file_to_access);
      return allowed_for_read_and_write;
    }
    default:
      return false;
  }
}

// Check if |requested_filename| can be opened with flags |requested_flags|.
// If |file_to_open| is not NULL, we will return the matching pointer from the
// whitelist. For paranoia, a caller should then use |file_to_open| rather
// than |requested_filename|, so that it never attempts to open an
// attacker-controlled file name, even if an attacker managed to fool the
// string comparison mechanism.
// Return true if opening should be allowed, false otherwise.
// Async signal safe if and only if |file_to_open| is NULL.
bool BrokerPolicy::GetFileNameIfAllowedToOpen(const char* requested_filename,
                                              int requested_flags,
                                              const char** file_to_open) const {
  if (!IsAllowedOpenFlags(requested_flags)) {
    return false;
  }
  switch (requested_flags & O_ACCMODE) {
    case O_RDONLY:
      return GetFileNameInWhitelist(
          allowed_r_files_, requested_filename, file_to_open);
    case O_WRONLY:
      return GetFileNameInWhitelist(
          allowed_w_files_, requested_filename, file_to_open);
    case O_RDWR: {
      bool allowed_for_read_and_write =
          GetFileNameInWhitelist(allowed_r_files_, requested_filename, NULL) &&
          GetFileNameInWhitelist(
              allowed_w_files_, requested_filename, file_to_open);
      return allowed_for_read_and_write;
    }
    default:
      return false;
  }
}

}  // namespace syscall_broker

}  // namespace sandbox
