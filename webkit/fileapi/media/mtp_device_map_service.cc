// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/mtp_device_map_service.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "webkit/fileapi/isolated_context.h"
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

void MTPDeviceMapService::AddDelegate(
    const FilePath::StringType& device_location,
    MTPDeviceDelegate* delegate) {
  DCHECK(delegate);
  DCHECK(!device_location.empty());
  base::AutoLock lock(lock_);
  if (ContainsKey(delegate_map_, device_location))
    return;

  delegate_map_[device_location] = delegate;
}

void MTPDeviceMapService::RemoveDelegate(
    const FilePath::StringType& device_location) {
  base::AutoLock lock(lock_);
  DelegateMap::iterator it = delegate_map_.find(device_location);
  DCHECK(it != delegate_map_.end());
  it->second->CancelPendingTasksAndDeleteDelegate();
  delegate_map_.erase(it);
}

MTPDeviceDelegate* MTPDeviceMapService::GetMTPDeviceDelegate(
    const std::string& filesystem_id) {
  FilePath device_path;
  if (!IsolatedContext::GetInstance()->GetRegisteredPath(filesystem_id,
                                                         &device_path)) {
    return NULL;
  }

  const FilePath::StringType& device_location = device_path.value();
  DCHECK(!device_location.empty());

  base::AutoLock lock(lock_);
  DelegateMap::const_iterator it = delegate_map_.find(device_location);
  return (it != delegate_map_.end()) ? it->second : NULL;
}

MTPDeviceMapService::MTPDeviceMapService() {
}

MTPDeviceMapService::~MTPDeviceMapService() {}

}  // namespace fileapi
