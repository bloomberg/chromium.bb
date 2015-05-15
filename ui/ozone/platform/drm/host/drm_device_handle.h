// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_HANDLE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_HANDLE_H_

#include "base/files/scoped_file.h"

namespace base {
class FilePath;
}

namespace ui {

class DrmDeviceHandle {
 public:
  DrmDeviceHandle();
  ~DrmDeviceHandle();

  int fd() const { return file_.get(); }

  bool Initialize(const base::FilePath& path);

  bool IsValid() const;
  base::ScopedFD PassFD();

 private:
  base::ScopedFD file_;

  DISALLOW_COPY_AND_ASSIGN(DrmDeviceHandle);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_HANDLE_H_
