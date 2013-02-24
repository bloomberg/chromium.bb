// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_SHAREABLE_FILE_REFERENCE_H_
#define WEBKIT_BLOB_SHAREABLE_FILE_REFERENCE_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class TaskRunner;
}

namespace webkit_blob {

// A refcounted wrapper around a FilePath that can optionally schedule
// the file to be deleted upon final release and/or to notify a consumer
// when final release occurs. This class is single-threaded and should
// only be invoked on the IO thread in chrome.
class WEBKIT_STORAGE_EXPORT ShareableFileReference
    : public base::RefCounted<ShareableFileReference> {
 public:
  typedef base::Callback<void(const base::FilePath&)> FinalReleaseCallback;

  enum FinalReleasePolicy {
    DELETE_ON_FINAL_RELEASE,
    DONT_DELETE_ON_FINAL_RELEASE,
  };

  // Returns a ShareableFileReference for the given path, if no reference
  // for this path exists returns NULL.
  static scoped_refptr<ShareableFileReference> Get(const base::FilePath& path);

  // Returns a ShareableFileReference for the given path, creating a new
  // reference if none yet exists. If there's a pre-existing reference for
  // the path, the deletable parameter of this method is ignored.
  static scoped_refptr<ShareableFileReference> GetOrCreate(
      const base::FilePath& path,
      FinalReleasePolicy policy,
      base::TaskRunner* file_task_runner);

  // The full file path.
  const base::FilePath& path() const { return path_; }

  // Whether it's to be deleted on final release.
  FinalReleasePolicy final_release_policy() const {
    return final_release_policy_;
  }

  void AddFinalReleaseCallback(const FinalReleaseCallback& callback);

 private:
  friend class base::RefCounted<ShareableFileReference>;

  ShareableFileReference(
      const base::FilePath& path,
      FinalReleasePolicy policy,
      base::TaskRunner* file_task_runner);
  ~ShareableFileReference();

  const base::FilePath path_;
  const FinalReleasePolicy final_release_policy_;
  const scoped_refptr<base::TaskRunner> file_task_runner_;
  std::vector<FinalReleaseCallback> final_release_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ShareableFileReference);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_SHAREABLE_FILE_REFERENCE_H_
