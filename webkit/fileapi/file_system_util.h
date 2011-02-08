// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
#pragma once

#include "webkit/fileapi/file_system_types.h"

class FilePath;
class GURL;

namespace fileapi {

bool CrackFileSystemURL(const GURL& url, GURL* origin_url, FileSystemType* type,
                        FilePath* file_path);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
