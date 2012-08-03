// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_MAP_SERVICE_H_
#define WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_MAP_SERVICE_H_

#include <map>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "webkit/fileapi/media/media_device_interface_impl.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

// Helper class to manage media device interfaces.
class FILEAPI_EXPORT MediaDeviceMapService {
 public:
  static MediaDeviceMapService* GetInstance();

  // Create or get media device interface associated with |filesystem_id|.
  // Return NULL if the |filesystem_id| is no longer valid (e.g. because the
  // corresponding device is detached etc). This function is called on IO
  // thread.
  MediaDeviceInterfaceImpl* CreateOrGetMediaDevice(
      const std::string& filesystem_id,
      base::SequencedTaskRunner* media_task_runner);

  // This function is called on UI thread.
  void RemoveMediaDevice(const FilePath::StringType& device_location);

 private:
  friend struct DefaultSingletonTraits<MediaDeviceMapService>;

  typedef scoped_refptr<MediaDeviceInterfaceImpl> MediaDeviceRefPtr;
  typedef std::map<FilePath::StringType, MediaDeviceRefPtr> MediaDeviceMap;

  // Get access to this class using GetInstance() method.
  MediaDeviceMapService();
  ~MediaDeviceMapService();

  base::Lock media_device_map_lock_;

  // Store a map of attached mtp devices.
  // Key: Device location.
  // Value: MtpDeviceInterface.
  MediaDeviceMap media_device_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceMapService);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_MAP_SERVICE_H_
