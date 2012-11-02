// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_MAP_SERVICE_H_
#define WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_MAP_SERVICE_H_

#include <map>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

class MTPDeviceDelegate;

// Helper class to manage media device delegates which can communicate with mtp
// devices to complete media file system operations.
class WEBKIT_STORAGE_EXPORT MTPDeviceMapService {
 public:
  static MTPDeviceMapService* GetInstance();

  // Adds the media device delegate for the given |device_location|. Called on
  // IO thread.
  void AddDelegate(const FilePath::StringType& device_location,
                   scoped_refptr<MTPDeviceDelegate> delegate);

  // Removes the media device delegate for the given |device_location| if
  // exists. Called on IO thread.
  void RemoveDelegate(const FilePath::StringType& device_location);

  // Gets the media device delegate associated with |filesystem_id|.
  // Return NULL if the |filesystem_id| is no longer valid (e.g. because the
  // corresponding device is detached etc). Called on IO thread.
  MTPDeviceDelegate* GetMTPDeviceDelegate(const std::string& filesystem_id);

 private:
  friend struct DefaultSingletonTraits<MTPDeviceMapService>;

  typedef scoped_refptr<MTPDeviceDelegate> MTPDeviceDelegateObj;

  // Mapping of device_location and MTPDeviceDelegate object.
  typedef std::map<FilePath::StringType, MTPDeviceDelegateObj> DelegateMap;

  // Get access to this class using GetInstance() method.
  MTPDeviceMapService();
  ~MTPDeviceMapService();

  // Stores a map of attached mtp device delegates.
  DelegateMap delegate_map_;

  // Stores a |thread_checker_| object to verify all methods of this class are
  // called on same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceMapService);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_MAP_SERVICE_H_
