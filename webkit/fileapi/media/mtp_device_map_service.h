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
#include "webkit/fileapi/fileapi_export.h"

namespace fileapi {

class MtpDeviceDelegate;

// Helper class to manage media device delegates which can communicate with mtp
// devices to complete media file system operations.
class FILEAPI_EXPORT MtpDeviceMapService {
 public:
  static MtpDeviceMapService* GetInstance();

  // Adds the media device delegate for the given |device_location|. Called on
  // IO thread.
  void AddDelegate(const FilePath::StringType& device_location,
                   scoped_refptr<MtpDeviceDelegate> delegate);

  // Removes the media device delegate for the given |device_location| if
  // exists. Called on IO thread.
  void RemoveDelegate(const FilePath::StringType& device_location);

  // Gets the media device delegate associated with |filesystem_id|.
  // Return NULL if the |filesystem_id| is no longer valid (e.g. because the
  // corresponding device is detached etc). Called on IO thread.
  MtpDeviceDelegate* GetMtpDeviceDelegate(const std::string& filesystem_id);

 private:
  friend struct DefaultSingletonTraits<MtpDeviceMapService>;

  typedef scoped_refptr<MtpDeviceDelegate> MtpDeviceDelegateObj;

  // Mapping of device_location and MtpDeviceDelegate object.
  typedef std::map<FilePath::StringType, MtpDeviceDelegateObj> DelegateMap;

  // Get access to this class using GetInstance() method.
  MtpDeviceMapService();
  ~MtpDeviceMapService();

  // Stores a map of attached mtp device delegates.
  DelegateMap delegate_map_;

  // Stores a |thread_checker_| object to verify all methods of this class are
  // called on same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MtpDeviceMapService);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_MAP_SERVICE_H_
