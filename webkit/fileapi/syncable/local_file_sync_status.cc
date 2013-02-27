// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_status.h"

#include "base/logging.h"

using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;

namespace sync_file_system {

LocalFileSyncStatus::LocalFileSyncStatus() {}

LocalFileSyncStatus::~LocalFileSyncStatus() {}

void LocalFileSyncStatus::StartWriting(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsChildOrParentSyncing(url));
  writing_[url]++;
}

void LocalFileSyncStatus::EndWriting(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  int count = --writing_[url];
  if (count == 0) {
    writing_.erase(url);
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSyncEnabled(url));
  }
}

void LocalFileSyncStatus::StartSyncing(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsChildOrParentWriting(url));
  DCHECK(!IsChildOrParentSyncing(url));
  syncing_.insert(url);
}

void LocalFileSyncStatus::EndSyncing(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  syncing_.erase(url);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnWriteEnabled(url));
}

bool LocalFileSyncStatus::IsWriting(const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  return IsChildOrParentWriting(url);
}

bool LocalFileSyncStatus::IsWritable(const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  return !IsChildOrParentSyncing(url);
}

bool LocalFileSyncStatus::IsSyncable(const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  return !IsChildOrParentSyncing(url) && !IsChildOrParentWriting(url);
}

void LocalFileSyncStatus::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void LocalFileSyncStatus::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

bool LocalFileSyncStatus::IsChildOrParentWriting(
    const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
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
  DCHECK(CalledOnValidThread());
  FileSystemURLSet::const_iterator upper = syncing_.upper_bound(url);
  FileSystemURLSet::const_reverse_iterator rupper(upper);
  if (upper != syncing_.end() && url.IsParent(*upper))
    return true;
  if (rupper != syncing_.rend() &&
      (*rupper == url || rupper->IsParent(url)))
    return true;
  return false;
}

}  // namespace sync_file_system
