// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

namespace content {

NativeFileSystemHandleBase::NativeFileSystemHandleBase(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    storage::IsolatedContext::ScopedFSHandle file_system)
    : manager_(manager),
      context_(context),
      url_(url),
      file_system_(std::move(file_system)) {
  DCHECK(manager_);
  DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
            file_system_.is_valid())
      << url_.mount_type();
}

NativeFileSystemHandleBase::~NativeFileSystemHandleBase() = default;

}  // namespace content