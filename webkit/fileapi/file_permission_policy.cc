// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_permission_policy.h"

#include "base/platform_file.h"

namespace fileapi {

const int kReadFilePermissions = base::PLATFORM_FILE_OPEN |
                                 base::PLATFORM_FILE_READ |
                                 base::PLATFORM_FILE_EXCLUSIVE_READ |
                                 base::PLATFORM_FILE_ASYNC;

const int kWriteFilePermissions = base::PLATFORM_FILE_OPEN |
                                  base::PLATFORM_FILE_WRITE |
                                  base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                                  base::PLATFORM_FILE_ASYNC |
                                  base::PLATFORM_FILE_WRITE_ATTRIBUTES;

const int kCreateFilePermissions = base::PLATFORM_FILE_CREATE;

const int kOpenFilePermissions = base::PLATFORM_FILE_CREATE |
                                 base::PLATFORM_FILE_OPEN_ALWAYS |
                                 base::PLATFORM_FILE_CREATE_ALWAYS |
                                 base::PLATFORM_FILE_OPEN_TRUNCATED |
                                 base::PLATFORM_FILE_WRITE |
                                 base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                                 base::PLATFORM_FILE_DELETE_ON_CLOSE |
                                 base::PLATFORM_FILE_WRITE_ATTRIBUTES;


}  // namespace fileapi
