// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_
#define UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"

namespace ui {

class DriSurfaceFactory;

class DriVSyncProvider : public gfx::VSyncProvider {
 public:
  DriVSyncProvider(DriSurfaceFactory* factory, gfx::AcceleratedWidget w);
  virtual ~DriVSyncProvider();

  virtual void GetVSyncParameters(const UpdateVSyncCallback& callback) OVERRIDE;

 private:
  DriSurfaceFactory* factory_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(DriVSyncProvider);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_
