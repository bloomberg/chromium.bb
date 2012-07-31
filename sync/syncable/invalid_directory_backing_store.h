// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_INVALID_DIRECTORY_BACKING_STORE_H_
#define SYNC_SYNCABLE_INVALID_DIRECTORY_BACKING_STORE_H_

#include "sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

// A class used to test scenarios where loading a directory fails.
class InvalidDirectoryBackingStore : public DirectoryBackingStore {
 public:
  InvalidDirectoryBackingStore();
  virtual ~InvalidDirectoryBackingStore();
  virtual DirOpenResult Load(
      MetahandlesIndex* entry_bucket,
      Directory::KernelLoadInfo* kernel_load_info) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(InvalidDirectoryBackingStore);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_INVALID_DIRECTORY_BACKING_STORE_H_
