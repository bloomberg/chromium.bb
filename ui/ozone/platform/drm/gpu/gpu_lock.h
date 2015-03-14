// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GPU_LOCK_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GPU_LOCK_H_

#include "base/macros.h"

namespace ui {

// Used to synchronize with Frecon and make sure the GPU process isn't trying to
// access the DRM resources before Frecon finishes drawing.
class GpuLock {
 public:
  GpuLock();
  ~GpuLock();

 private:
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(GpuLock);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GPU_LOCK_H_
