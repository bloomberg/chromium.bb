// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_IMPL_DRM_VSYNC_PROVIDER_H_
#define UI_OZONE_PLATFORM_IMPL_DRM_VSYNC_PROVIDER_H_

#include "ui/gfx/vsync_provider.h"

namespace ui {

class DrmWindow;

class DrmVSyncProvider : public gfx::VSyncProvider {
 public:
  DrmVSyncProvider(DrmWindow* window_delegate);
  ~DrmVSyncProvider() override;

  void GetVSyncParameters(const UpdateVSyncCallback& callback) override;

 private:
  DrmWindow* window_delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DrmVSyncProvider);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_IMPL_DRM_VSYNC_PROVIDER_H_
