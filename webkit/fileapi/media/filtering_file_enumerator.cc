// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "webkit/fileapi/media/filtering_file_enumerator.h"

namespace fileapi {

FilteringFileEnumerator::FilteringFileEnumerator(
    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> base_enumerator,
    MediaPathFilter* filter)
    : base_enumerator_(base_enumerator.Pass()),
      filter_(filter) {
  DCHECK(base_enumerator_.get());
  DCHECK(filter);
}

FilteringFileEnumerator::~FilteringFileEnumerator() {
}

FilePath FilteringFileEnumerator::Next() {
  while (true) {
    FilePath next = base_enumerator_->Next();
    if (next.empty() ||
        base_enumerator_->IsDirectory() ||
        filter_->Match(next))
      return next;
  }
}

int64 FilteringFileEnumerator::Size() {
  return base_enumerator_->Size();
}

base::Time FilteringFileEnumerator::LastModifiedTime() {
  return base_enumerator_->LastModifiedTime();
}

bool FilteringFileEnumerator::IsDirectory() {
  return base_enumerator_->IsDirectory();
}

}  // namespace fileapi
