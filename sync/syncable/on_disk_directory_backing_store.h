// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_ON_DISK_DIRECTORY_BACKING_STORE_H_
#define SYNC_SYNCABLE_ON_DISK_DIRECTORY_BACKING_STORE_H_
#pragma once

#include "base/file_path.h"
#include "sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

// This is the concrete class that provides a useful implementation of
// DirectoryBackingStore.
class OnDiskDirectoryBackingStore : public DirectoryBackingStore {
 public:
  OnDiskDirectoryBackingStore(const std::string& dir_name,
                              const FilePath& backing_filepath);
  virtual DirOpenResult Load(
      MetahandlesIndex* entry_bucket,
      Directory::KernelLoadInfo* kernel_load_info) OVERRIDE;

 private:
  FilePath backing_filepath_;
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_ON_DISK_DIRECTORY_BACKING_STORE_H_
