// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_status.h"

namespace fileapi {

LocalFileSyncStatus::LocalFileSyncStatus() {}

LocalFileSyncStatus::~LocalFileSyncStatus() {
  base::AutoLock lock(lock_);
  syncing_.clear();
  writing_.clear();
}

bool LocalFileSyncStatus::TryIncrementWriting(const FileSystemURL& url) {
  base::AutoLock lock(lock_);
  if (IsChildOrParentSyncing(url))
    return false;
  writing_[url]++;
  return true;
}

void LocalFileSyncStatus::DecrementWriting(const FileSystemURL& url) {
  base::AutoLock lock(lock_);
  int count = --writing_[url];
  if (count == 0) {
    writing_.erase(url);
    // TODO(kinuko): fire NeedsSynchronization notification.
  }
}

bool LocalFileSyncStatus::TryDisableWriting(const FileSystemURL& url) {
  base::AutoLock lock(lock_);
  if (IsChildOrParentWriting(url))
    return false;
  syncing_.insert(url);
  return true;
}

void LocalFileSyncStatus::EnableWriting(const FileSystemURL& url) {
  base::AutoLock lock(lock_);
  syncing_.erase(url);
  // TODO(kinuko): fire WriteEnabled notification.
}


bool LocalFileSyncStatus::IsWriting(const FileSystemURL& url) const {
  base::AutoLock lock(lock_);
  return IsChildOrParentWriting(url);
}

bool LocalFileSyncStatus::IsWritable(const FileSystemURL& url) const {
  base::AutoLock lock(lock_);
  return !IsChildOrParentSyncing(url);
}

bool LocalFileSyncStatus::IsChildOrParentWriting(
    const FileSystemURL& url) const {
  lock_.AssertAcquired();
  URLCountMap::const_iterator upper = writing_.upper_bound(url);
  URLCountMap::const_reverse_iterator rupper(upper);
  if (upper != writing_.end() && url.IsParent(upper->first))
    return true;
  if (rupper != writing_.rend() &&
      (rupper->first == url || rupper->first.IsParent(url)))
    return true;
  return false;
}

bool LocalFileSyncStatus::IsChildOrParentSyncing(
    const FileSystemURL& url) const {
  lock_.AssertAcquired();
  URLSet::const_iterator upper = syncing_.upper_bound(url);
  URLSet::const_reverse_iterator rupper(upper);
  if (upper != syncing_.end() && url.IsParent(*upper))
    return true;
  if (rupper != syncing_.rend() &&
      (*rupper == url || rupper->IsParent(url)))
    return true;
  return false;
}

}  // namespace fileapi
