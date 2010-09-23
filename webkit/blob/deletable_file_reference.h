// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_DELETABLE_FILE_REFERENCE_H_
#define WEBKIT_BLOB_DELETABLE_FILE_REFERENCE_H_
#pragma once

#include "base/file_path.h"
#include "base/ref_counted.h"

namespace base {
class MessageLoopProxy;
}

namespace webkit_blob {

// A refcounted wrapper around a FilePath that schedules the file
// to be deleted upon final release.
class DeletableFileReference : public base::RefCounted<DeletableFileReference> {
 public:
  // Returns a DeletableFileReference for the given path, if no reference
  // for this path exists returns NULL.
  static scoped_refptr<DeletableFileReference> Get(const FilePath& path);

  // Returns a DeletableFileReference for the given path, creating a new
  // reference if none yet exists.
  static scoped_refptr<DeletableFileReference> GetOrCreate(
      const FilePath& path, base::MessageLoopProxy* file_thread);

  // The full file path.
  const FilePath& path() const { return path_; }

 private:
  friend class base::RefCounted<DeletableFileReference>;

  DeletableFileReference(
      const FilePath& path, base::MessageLoopProxy* file_thread);
  ~DeletableFileReference();

  const FilePath path_;
  scoped_refptr<base::MessageLoopProxy> file_thread_;

  DISALLOW_COPY_AND_ASSIGN(DeletableFileReference);
};

}  // namespace webkit_blob

#endif  // BASE_DELETABLE_FILE_REFERENCE_H_
