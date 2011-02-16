// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_manager.h"
#include "webkit/fileapi/file_system_usage_tracker.h"

namespace fileapi {

FileSystemContext::FileSystemContext(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    const FilePath& profile_path,
    bool is_incognito,
    bool allow_file_access,
    bool unlimited_quota)
    : file_message_loop_(file_message_loop),
      io_message_loop_(io_message_loop),
      path_manager_(new FileSystemPathManager(
          file_message_loop, profile_path, is_incognito, allow_file_access)),
      quota_manager_(new FileSystemQuotaManager(
          allow_file_access, unlimited_quota)),
      usage_tracker_(new FileSystemUsageTracker(
          file_message_loop, profile_path, is_incognito)) {
}

FileSystemContext::~FileSystemContext() {
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

void FileSystemContext::SetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  quota_manager()->SetOriginQuotaUnlimited(url);
}

void FileSystemContext::ResetOriginQuotaUnlimited(const GURL& url) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  quota_manager()->ResetOriginQuotaUnlimited(url);
}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!io_message_loop_->BelongsToCurrentThread()) {
    io_message_loop_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace fileapi
