// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"

namespace content {

NativeFileSystemTransferTokenImpl::NativeFileSystemTransferTokenImpl(
    const storage::FileSystemURL& url,
    storage::IsolatedContext::ScopedFSHandle file_system,
    HandleType type)
    : token_(base::UnguessableToken::Create()),
      url_(url),
      file_system_(std::move(file_system)),
      type_(type) {
  DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
            file_system_.is_valid())
      << url_.mount_type();
}

void NativeFileSystemTransferTokenImpl::GetInternalID(
    GetInternalIDCallback callback) {
  std::move(callback).Run(token_);
}

}  // namespace content
