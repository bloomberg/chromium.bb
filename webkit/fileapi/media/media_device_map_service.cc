// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/media_device_map_service.h"

#include <utility>

#include "webkit/fileapi/isolated_context.h"

namespace fileapi {

using base::SequencedTaskRunner;

// static
MediaDeviceMapService* MediaDeviceMapService::GetInstance() {
  return Singleton<MediaDeviceMapService>::get();
}

MediaDeviceInterfaceImpl* MediaDeviceMapService::CreateOrGetMediaDevice(
    const std::string& filesystem_id,
    SequencedTaskRunner* media_task_runner) {
  DCHECK(media_task_runner);

  base::AutoLock lock(media_device_map_lock_);

  FilePath device_path;
  if (!IsolatedContext::GetInstance()->GetRegisteredPath(filesystem_id,
                                                         &device_path)) {
    return NULL;
  }

  FilePath::StringType device_location = device_path.value();
  DCHECK(!device_location.empty());

  MediaDeviceMap::const_iterator it = media_device_map_.find(device_location);
  if (it == media_device_map_.end()) {
    media_device_map_.insert(std::make_pair(
        device_location, new MediaDeviceInterfaceImpl(device_location,
                                                      media_task_runner)));
  }
  return media_device_map_[device_location].get();
}

void MediaDeviceMapService::RemoveMediaDevice(
    const std::string& device_location) {
  base::AutoLock lock(media_device_map_lock_);
  MediaDeviceMap::iterator it = media_device_map_.find(device_location);
  if (it != media_device_map_.end())
    media_device_map_.erase(it);
}

MediaDeviceMapService::MediaDeviceMapService() {
}

MediaDeviceMapService::~MediaDeviceMapService() {
}

}  // namespace fileapi
