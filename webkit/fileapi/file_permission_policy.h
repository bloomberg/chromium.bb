// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_PERMISSION_POLICY_H_
#define WEBKIT_FILEAPI_FILE_PERMISSION_POLICY_H_

#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

WEBKIT_STORAGE_EXPORT extern const int kReadFilePermissions;
WEBKIT_STORAGE_EXPORT extern const int kWriteFilePermissions;
WEBKIT_STORAGE_EXPORT extern const int kCreateFilePermissions;
WEBKIT_STORAGE_EXPORT extern const int kOpenFilePermissions;

enum FilePermissionPolicy {
  // Any access should be always denied.
  FILE_PERMISSION_ALWAYS_DENY,

  // Any access should be always allowed. (This should be used only for
  // access to sandbox directories.)
  FILE_PERMISSION_ALWAYS_ALLOW,

  // Access should be examined by per-file permission policy.
  FILE_PERMISSION_USE_FILE_PERMISSION,

  // Access should be examined by per-filesystem permission policy.
  FILE_PERMISSION_USE_FILESYSTEM_PERMISSION,
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_PERMISSION_POLICY_H_
