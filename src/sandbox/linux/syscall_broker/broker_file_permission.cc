// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_file_permission.h"

#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <ostream>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

namespace sandbox {
namespace syscall_broker {

BrokerFilePermission::BrokerFilePermission(BrokerFilePermission&&) = default;
BrokerFilePermission& BrokerFilePermission::operator=(BrokerFilePermission&&) =
    default;
BrokerFilePermission::BrokerFilePermission(const BrokerFilePermission&) =
    default;
BrokerFilePermission& BrokerFilePermission::operator=(
    const BrokerFilePermission&) = default;

BrokerFilePermission::~BrokerFilePermission() = default;

bool BrokerFilePermission::ValidatePath(const char* path) {
  if (!path)
    return false;

  const size_t len = strlen(path);
  // No empty paths
  if (len == 0)
    return false;
  // Paths must be absolute and not relative
  if (path[0] != '/')
    return false;
  // No trailing / (but "/" is valid)
  if (len > 1 && path[len - 1] == '/')
    return false;
  // No trailing /..
  if (len >= 3 && path[len - 3] == '/' && path[len - 2] == '.' &&
      path[len - 1] == '.')
    return false;
  // No /../ anywhere
  for (size_t i = 0; i < len; i++) {
    if (path[i] == '/' && (len - i) > 3) {
      if (path[i + 1] == '.' && path[i + 2] == '.' && path[i + 3] == '/') {
        return false;
      }
    }
  }
  return true;
}

// Async signal safe
// Calls std::string::c_str(), strncmp and strlen. All these
// methods are async signal safe in common standard libs.
// TODO(leecam): remove dependency on std::string
bool BrokerFilePermission::MatchPath(const char* requested_filename) const {
  // Note: This recursive match will allow any path under the allowlisted
  // path, for any number of directory levels. E.g. if the allowlisted
  // path is /good/ then the following will be permitted by the policy.
  //   /good/file1
  //   /good/folder/file2
  //   /good/folder/folder2/file3
  // If an attacker could make 'folder' a symlink to ../../ they would have
  // access to the entire filesystem.
  // Allowlisting with multiple depths is useful, e.g /proc/ but
  // the system needs to ensure symlinks can not be created!
  // That said if an attacker can convert any of the absolute paths
  // to a symlink they can control any file on the system also.
  return recursive()
             ? strncmp(requested_filename, path_.c_str(), path_.length()) == 0
             : strcmp(requested_filename, path_.c_str()) == 0;
}

// Async signal safe.
// External call to std::string::c_str() is
// called in MatchPath.
// TODO(leecam): remove dependency on std::string
bool BrokerFilePermission::CheckAccess(const char* requested_filename,
                                       int mode,
                                       const char** file_to_access) const {
  // First, check if |mode| is existence, ability to read or ability
  // to write. We do not support X_OK.
  if (mode != F_OK && mode & ~(R_OK | W_OK))
    return false;

  if (!ValidatePath(requested_filename))
    return false;

  return CheckAccessInternal(requested_filename, mode, file_to_access);
}

bool BrokerFilePermission::CheckAccessInternal(
    const char* requested_filename,
    int mode,
    const char** file_to_access) const {
  if (!MatchPath(requested_filename))
    return false;

  bool allowed = false;
  switch (mode) {
    case F_OK:
      allowed = allow_read() || allow_write();
      break;
    case R_OK:
      allowed = allow_read();
      break;
    case W_OK:
      allowed = allow_write();
      break;
    case R_OK | W_OK:
      allowed = allow_read() && allow_write();
      break;
    default:
      break;
  }
  if (!allowed)
    return false;

  if (file_to_access)
    *file_to_access = recursive() ? requested_filename : path_.c_str();

  return true;
}

// Async signal safe.
// External call to std::string::c_str() is
// called in MatchPath.
// TODO(leecam): remove dependency on std::string
bool BrokerFilePermission::CheckOpen(const char* requested_filename,
                                     int flags,
                                     const char** file_to_open,
                                     bool* unlink_after_open) const {
  if (!ValidatePath(requested_filename))
    return false;

  if (!MatchPath(requested_filename))
    return false;

  // First, check the access mode is valid.
  const int access_mode = flags & O_ACCMODE;
  if (access_mode != O_RDONLY && access_mode != O_WRONLY &&
      access_mode != O_RDWR) {
    return false;
  }

  // Check if read is allowed.
  if (!allow_read() && (access_mode == O_RDONLY || access_mode == O_RDWR)) {
    return false;
  }

  // Check if write is allowed.
  if (!allow_write() && (access_mode == O_WRONLY || access_mode == O_RDWR)) {
    return false;
  }

  // Check if file creation is allowed.
  if (!allow_create() && (flags & O_CREAT)) {
    return false;
  }

  // If this file is to be temporary, ensure it is created, not pre-existing.
  // See https://crbug.com/415681#c17
  if (temporary_only() && (!(flags & O_CREAT) || !(flags & O_EXCL))) {
    return false;
  }

  // Some flags affect the behavior of the current process. We don't support
  // them and don't allow them for now.
  if (flags & kCurrentProcessOpenFlagsMask) {
    return false;
  }

  // The effect of (O_RDONLY | O_TRUNC) is undefined, and in some cases it
  // actually truncates, so deny.
  if (access_mode == O_RDONLY && (flags & O_TRUNC) != 0) {
    return false;
  }

  // Now check that all the flags are known to us.
  const int creation_and_status_flags = flags & ~O_ACCMODE;
  const int known_flags = O_APPEND | O_ASYNC | O_CLOEXEC | O_CREAT | O_DIRECT |
                          O_DIRECTORY | O_EXCL | O_LARGEFILE | O_NOATIME |
                          O_NOCTTY | O_NOFOLLOW | O_NONBLOCK | O_NDELAY |
                          O_SYNC | O_TRUNC;

  const int unknown_flags = ~known_flags;
  const bool has_unknown_flags = creation_and_status_flags & unknown_flags;
  if (has_unknown_flags)
    return false;

  if (file_to_open)
    *file_to_open = recursive() ? requested_filename : path_.c_str();

  if (unlink_after_open)
    *unlink_after_open = temporary_only();

  return true;
}

bool BrokerFilePermission::CheckStat(const char* requested_filename,
                                     const char** file_to_access) const {
  if (!ValidatePath(requested_filename))
    return false;

  // Ability to access implies ability to stat().
  if (CheckAccessInternal(requested_filename, F_OK, file_to_access))
    return true;

  // Allow stat() on leading directories if have create or stat() permission.
  if (!(allow_create() || allow_stat_with_intermediates()))
    return false;

  // NOTE: ValidatePath proved requested_length != 0;
  size_t requested_length = strlen(requested_filename);
  CHECK(requested_length);

  // Special case for root: only one slash, otherwise must have a second
  // slash in the right spot to avoid substring matches.
  // |allow_stat_with_intermediates()| can match on the full path, and
  // |allow_create()| only matches a leading directory.
  if ((requested_length == 1 && requested_filename[0] == '/') ||
      (allow_stat_with_intermediates() && path_ == requested_filename) ||
      (requested_length < path_.length() &&
       memcmp(path_.c_str(), requested_filename, requested_length) == 0 &&
       path_.c_str()[requested_length] == '/')) {
    if (file_to_access)
      *file_to_access = requested_filename;

    return true;
  }

  return false;
}

const char* BrokerFilePermission::GetErrorMessageForTests() {
  return "Invalid BrokerFilePermission";
}

void BrokerFilePermission::DieOnInvalidPermission() {
  // Must have enough length for a '/'
  CHECK(path_.length() > 0) << GetErrorMessageForTests();

  // Allowlisted paths must be absolute.
  CHECK(path_[0] == '/') << GetErrorMessageForTests();

  // Don't allow temporary creation without create permission.
  if (temporary_only())
    CHECK(allow_create()) << GetErrorMessageForTests();

  // Recursive paths must have a trailing slash, absolutes must not.
  const char last_char = *(path_.rbegin());
  if (recursive())
    CHECK(last_char == '/') << GetErrorMessageForTests();
  else
    CHECK(last_char != '/') << GetErrorMessageForTests();
}

BrokerFilePermission::BrokerFilePermission(std::string path, uint64_t flags)
    : path_(std::move(path)), flags_(flags) {
  DieOnInvalidPermission();
}

BrokerFilePermission::BrokerFilePermission(
    std::string path,
    RecursionOption recurse_opt,
    PersistenceOption persist_opt,
    ReadPermission read_perm,
    WritePermission write_perm,
    CreatePermission create_perm,
    StatWithIntermediatesPermission stat_perm)
    : path_(std::move(path)) {
  flags_[kRecursiveBitPos] = recurse_opt == RecursionOption::kRecursive;
  flags_[kTemporaryOnlyBitPos] =
      persist_opt == PersistenceOption::kTemporaryOnly;
  flags_[kAllowReadBitPos] = read_perm == ReadPermission::kAllowRead;
  flags_[kAllowWriteBitPos] = write_perm == WritePermission::kAllowWrite;
  flags_[kAllowCreateBitPos] = create_perm == CreatePermission::kAllowCreate;
  flags_[kAllowStatWithIntermediatesBitPos] =
      stat_perm == StatWithIntermediatesPermission::kAllowStatWithIntermediates;

  DieOnInvalidPermission();
}

}  // namespace syscall_broker
}  // namespace sandbox
