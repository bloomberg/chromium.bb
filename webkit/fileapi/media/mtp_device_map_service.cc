// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/mtp_device_map_service.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/media/mtp_device_async_delegate.h"
#include "webkit/fileapi/media/mtp_device_delegate.h"

namespace fileapi {

namespace {

base::LazyInstance<MTPDeviceMapService> g_mtp_device_map_service =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MTPDeviceMapService* MTPDeviceMapService::GetInstance() {
  return g_mtp_device_map_service.Pointer();
}

/////////////////////////////////////////////////////////////////////////////
//   Following methods are used to manage MTPDeviceAsyncDelegate objects.  //
/////////////////////////////////////////////////////////////////////////////
void MTPDeviceMapService::AddAsyncDelegate(
    const base::FilePath::StringType& device_location,
    MTPDeviceAsyncDelegate* delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(delegate);
  DCHECK(!device_location.empty());
  if (ContainsKey(async_delegate_map_, device_location))
    return;
  async_delegate_map_[device_location] = delegate;
}

void MTPDeviceMapService::RemoveAsyncDelegate(
    const base::FilePath::StringType& device_location) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AsyncDelegateMap::iterator it = async_delegate_map_.find(device_location);
  DCHECK(it != async_delegate_map_.end());
  it->second->CancelPendingTasksAndDeleteDelegate();
  async_delegate_map_.erase(it);
}

MTPDeviceAsyncDelegate* MTPDeviceMapService::GetMTPDeviceAsyncDelegate(
    const std::string& filesystem_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath device_path;
  if (!IsolatedContext::GetInstance()->GetRegisteredPath(filesystem_id,
                                                         &device_path)) {
    return NULL;
  }

  const base::FilePath::StringType& device_location = device_path.value();
  DCHECK(!device_location.empty());
  AsyncDelegateMap::const_iterator it =
      async_delegate_map_.find(device_location);
  return (it != async_delegate_map_.end()) ? it->second : NULL;
}


/////////////////////////////////////////////////////////////////////////////
//  Following methods are used to manage synchronous MTPDeviceDelegate     //
//  objects.                                                               //
//  TODO(kmadhusu): Remove the synchronous interfaces after fixing         //
//  crbug.com/154835                                                       //
/////////////////////////////////////////////////////////////////////////////
void MTPDeviceMapService::AddDelegate(
    const base::FilePath::StringType& device_location,
    MTPDeviceDelegate* delegate) {
  DCHECK(delegate);
  DCHECK(!device_location.empty());
  base::AutoLock lock(lock_);
  if (ContainsKey(sync_delegate_map_, device_location))
    return;

  sync_delegate_map_[device_location] = delegate;
}

void MTPDeviceMapService::RemoveDelegate(
    const base::FilePath::StringType& device_location) {
  base::AutoLock lock(lock_);
  SyncDelegateMap::iterator it = sync_delegate_map_.find(device_location);
  DCHECK(it != sync_delegate_map_.end());
  it->second->CancelPendingTasksAndDeleteDelegate();
  sync_delegate_map_.erase(it);
}

MTPDeviceDelegate* MTPDeviceMapService::GetMTPDeviceDelegate(
    const std::string& filesystem_id) {
  base::FilePath device_path;
  if (!IsolatedContext::GetInstance()->GetRegisteredPath(filesystem_id,
                                                         &device_path)) {
    return NULL;
  }

  const base::FilePath::StringType& device_location = device_path.value();
  DCHECK(!device_location.empty());
  base::AutoLock lock(lock_);
  SyncDelegateMap::const_iterator it = sync_delegate_map_.find(device_location);
  return (it != sync_delegate_map_.end()) ? it->second : NULL;
}

MTPDeviceMapService::MTPDeviceMapService() {
}

MTPDeviceMapService::~MTPDeviceMapService() {}

}  // namespace fileapi
