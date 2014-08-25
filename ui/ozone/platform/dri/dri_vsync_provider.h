// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_
#define UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_

#include "ui/gfx/vsync_provider.h"

namespace ui {

class DriWindowDelegate;

class DriVSyncProvider : public gfx::VSyncProvider {
 public:
  DriVSyncProvider(DriWindowDelegate* window_delegate);
  virtual ~DriVSyncProvider();

  virtual void GetVSyncParameters(const UpdateVSyncCallback& callback) OVERRIDE;

 private:
  DriWindowDelegate* window_delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DriVSyncProvider);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_
