// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MEDIA_PATH_FILTER_H_
#define WEBKIT_FILEAPI_MEDIA_MEDIA_PATH_FILTER_H_

#include <vector>

#include "base/file_path.h"
#include "base/synchronization/lock.h"
#include "webkit/fileapi/fileapi_export.h"

class FilePath;

namespace fileapi {

// This class holds the list of file path extensions that we should expose on
// media filesystem.
class FILEAPI_EXPORT MediaPathFilter {
 public:
  MediaPathFilter();
  ~MediaPathFilter();
  bool Match(const FilePath& path);

 private:
  typedef std::vector<FilePath::StringType> MediaFileExtensionList;

  void EnsureInitialized();

  bool initialized_;
  base::Lock initialization_lock_;
  MediaFileExtensionList media_file_extensions_;

  DISALLOW_COPY_AND_ASSIGN(MediaPathFilter);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MEDIA_PATH_FILTER_H_
