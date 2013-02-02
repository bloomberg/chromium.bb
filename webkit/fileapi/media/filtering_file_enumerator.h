// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_FILTERING_FILE_ENUMERATOR_H_
#define WEBKIT_FILEAPI_MEDIA_FILTERING_FILE_ENUMERATOR_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

// This class wraps another file enumerator and filters out non-media files
// from its result, refering given MediaPathFilter.
class WEBKIT_STORAGE_EXPORT FilteringFileEnumerator
    : public NON_EXPORTED_BASE(FileSystemFileUtil::AbstractFileEnumerator) {
 public:
  FilteringFileEnumerator(
      scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> base_enumerator,
      MediaPathFilter* filter);
  virtual ~FilteringFileEnumerator();

  virtual base::FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;

 private:
  // The file enumerator to be wrapped.
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> base_enumerator_;

  // Path filter to filter out non-Media files.
  MediaPathFilter* filter_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_FILTERING_FILE_ENUMERATOR_H_
