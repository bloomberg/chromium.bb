// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_IMPL_DRI_VSYNC_PROVIDER_H_
#define UI_GFX_OZONE_IMPL_DRI_VSYNC_PROVIDER_H_

#include "ui/gfx/vsync_provider.h"

namespace gfx {

class HardwareDisplayController;

class DriVSyncProvider : public VSyncProvider {
 public:
  DriVSyncProvider(HardwareDisplayController* controller);
  virtual ~DriVSyncProvider();

  virtual void GetVSyncParameters(const UpdateVSyncCallback& callback) OVERRIDE;

 private:
  HardwareDisplayController* controller_;

  DISALLOW_COPY_AND_ASSIGN(DriVSyncProvider);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_IMPL_DRI_VSYNC_PROVIDER_H_
