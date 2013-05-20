// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_DIRECTORY_ENTRY_H_
#define WEBKIT_FILEAPI_DIRECTORY_ENTRY_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time.h"

namespace fileapi {

// Holds metadata for file or directory entry.
struct DirectoryEntry {
  base::FilePath::StringType name;
  bool is_directory;
  int64 size;
  base::Time last_modified_time;
};

}

#endif  // WEBKIT_FILEAPI_DIRECTORY_ENTRY_H_
