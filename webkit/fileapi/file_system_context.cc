// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_usage_tracker.h"

namespace fileapi {

FileSystemContext::FileSystemContext(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
    const FilePath& profile_path,
    bool is_incognito,
    bool allow_file_access,
    bool unlimited_quota)
    : file_message_loop_(file_message_loop),
      io_message_loop_(io_message_loop),
      special_storage_policy_(special_storage_policy),
      allow_file_access_from_files_(allow_file_access),
      unlimited_quota_(unlimited_quota),
      path_manager_(new FileSystemPathManager(
          file_message_loop, profile_path, is_incognito, allow_file_access)),
      usage_tracker_(new FileSystemUsageTracker(
          file_message_loop, profile_path, is_incognito)) {
}

FileSystemContext::~FileSystemContext() {
}

bool FileSystemContext::IsStorageUnlimited(const GURL& origin) {
  // If allow-file-access-from-files flag is explicitly given and the scheme
  // is file, or if unlimited quota for this process was explicitly requested,
  // return true.
  return unlimited_quota_ ||
      (allow_file_access_from_files_ && origin.SchemeIsFile()) ||
      (special_storage_policy_.get() &&
          special_storage_policy_->IsStorageUnlimited(origin));
}

void FileSystemContext::DeleteDataForOriginOnFileThread(
    const GURL& origin_url) {
  DCHECK(path_manager_.get());
  DCHECK(file_message_loop_->BelongsToCurrentThread());

  std::string origin_identifier =
      FileSystemPathManager::GetOriginIdentifierFromURL(origin_url);
  FilePath path_for_origin = path_manager_->base_path().AppendASCII(
      origin_identifier);

  file_util::Delete(path_for_origin, true /* recursive */);
}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!io_message_loop_->BelongsToCurrentThread()) {
    io_message_loop_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace fileapi
