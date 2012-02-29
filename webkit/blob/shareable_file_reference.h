// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_SHAREABLE_FILE_REFERENCE_H_
#define WEBKIT_BLOB_SHAREABLE_FILE_REFERENCE_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "webkit/blob/blob_export.h"

namespace base {
class MessageLoopProxy;
}

namespace webkit_blob {

// A refcounted wrapper around a FilePath that can optionally schedule
// the file to be deleted upon final release and/or to notify a consumer
// when final release occurs. This class is single-threaded and should
// only be invoked on the IO thread in chrome.
class BLOB_EXPORT ShareableFileReference
    : public base::RefCounted<ShareableFileReference> {
 public:
  typedef base::Callback<void(const FilePath&)> FinalReleaseCallback;

  enum FinalReleasePolicy {
    DELETE_ON_FINAL_RELEASE,
    DONT_DELETE_ON_FINAL_RELEASE,
  };

  // Returns a ShareableFileReference for the given path, if no reference
  // for this path exists returns NULL.
  static scoped_refptr<ShareableFileReference> Get(const FilePath& path);

  // Returns a ShareableFileReference for the given path, creating a new
  // reference if none yet exists. If there's a pre-existing reference for
  // the path, the deletable parameter of this method is ignored.
  static scoped_refptr<ShareableFileReference> GetOrCreate(
      const FilePath& path,
      FinalReleasePolicy policy,
      base::MessageLoopProxy* file_thread);

  // The full file path.
  const FilePath& path() const { return path_; }

  // Whether it's to be deleted on final release.
  FinalReleasePolicy final_release_policy() const {
    return final_release_policy_;
  }

  void AddFinalReleaseCallback(const FinalReleaseCallback& callback);

 private:
  friend class base::RefCounted<ShareableFileReference>;

  ShareableFileReference(
      const FilePath& path,
      FinalReleasePolicy policy,
      base::MessageLoopProxy* file_thread);
  ~ShareableFileReference();

  const FilePath path_;
  const FinalReleasePolicy final_release_policy_;
  const scoped_refptr<base::MessageLoopProxy> file_thread_;
  std::vector<FinalReleaseCallback> final_release_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ShareableFileReference);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_SHAREABLE_FILE_REFERENCE_H_
