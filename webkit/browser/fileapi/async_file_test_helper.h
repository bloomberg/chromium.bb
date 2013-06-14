// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_ASYNC_FILE_TEST_HELPER_H_
#define WEBKIT_BROWSER_FILEAPI_ASYNC_FILE_TEST_HELPER_H_

#include "base/basictypes.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/quota/quota_status_code.h"

namespace quota {
class QuotaManager;
}

namespace fileapi {

class FileSystemContext;
class FileSystemURL;

// A helper class to perform async file operations in a synchronous way.
class AsyncFileTestHelper {
 public:
  typedef FileSystemOperation::FileEntryList FileEntryList;

  static const int64 kDontCheckSize;

  // Performs Copy from |src| to |dest| and returns the status code.
  static base::PlatformFileError Copy(FileSystemContext* context,
                                      const FileSystemURL& src,
                                      const FileSystemURL& dest);

  // Performs Move from |src| to |dest| and returns the status code.
  static base::PlatformFileError Move(FileSystemContext* context,
                                      const FileSystemURL& src,
                                      const FileSystemURL& dest);

  // Removes the given |url|.
  static base::PlatformFileError Remove(FileSystemContext* context,
                                        const FileSystemURL& url,
                                        bool recursive);

  // Performs ReadDirectory on |url|.
  static base::PlatformFileError ReadDirectory(FileSystemContext* context,
                                               const FileSystemURL& url,
                                               FileEntryList* entries);

  // Creates a directory at |url|.
  static base::PlatformFileError CreateDirectory(FileSystemContext* context,
                                                 const FileSystemURL& url);

  // Creates a file at |url|.
  static base::PlatformFileError CreateFile(FileSystemContext* context,
                                            const FileSystemURL& url);

  // Truncates the file |url| to |size|.
  static base::PlatformFileError TruncateFile(FileSystemContext* context,
                                              const FileSystemURL& url,
                                              size_t size);

  // Retrieves PlatformFileInfo for |url| and populates |file_info|.
  static base::PlatformFileError GetMetadata(FileSystemContext* context,
                                             const FileSystemURL& url,
                                             base::PlatformFileInfo* file_info);

  // Retrieves FilePath for |url| and populates |platform_path|.
  static base::PlatformFileError GetPlatformPath(FileSystemContext* context,
                                                 const FileSystemURL& url,
                                                 base::FilePath* platform_path);

  // Returns true if a file exists at |url| with |size|. If |size| is
  // kDontCheckSize it doesn't check the file size (but just check its
  // existence).
  static bool FileExists(FileSystemContext* context,
                         const FileSystemURL& url,
                         int64 size);

  // Returns true if a directory exists at |url|.
  static bool DirectoryExists(FileSystemContext* context,
                              const FileSystemURL& url);

  // Returns usage and quota. It's valid to pass NULL to |usage| and/or |quota|.
  static quota::QuotaStatusCode GetUsageAndQuota(
      quota::QuotaManager* quota_manager,
      const GURL& origin,
      FileSystemType type,
      int64* usage,
      int64* quota);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_ASYNC_FILE_TEST_HELPER_H_
