// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandboxed_file_system_context.h"

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_manager.h"

namespace fileapi {

SandboxedFileSystemContext::SandboxedFileSystemContext(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path,
    bool is_incognito,
    bool allow_file_access,
    bool unlimited_quota)
    : path_manager_(new FileSystemPathManager(
          file_message_loop, profile_path, is_incognito, allow_file_access)),
      quota_manager_(new FileSystemQuotaManager(allow_file_access,
                                                unlimited_quota)) {
}

SandboxedFileSystemContext::~SandboxedFileSystemContext() {
}

void SandboxedFileSystemContext::Shutdown() {
  path_manager_.reset();
  quota_manager_.reset();
}

}  // namespace fileapi
