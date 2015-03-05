// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_SNAPSHOT_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_SNAPSHOT_H_

#include "base/memory/ref_counted.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/platform/drm/gpu/scoped_drm_types.h"

namespace ui {

class DrmDevice;

class DrmDisplaySnapshot : public DisplaySnapshot {
 public:
  DrmDisplaySnapshot(const scoped_refptr<DrmDevice>& drm,
                     drmModeConnector* connector,
                     drmModeCrtc* crtc,
                     uint32_t index);
  ~DrmDisplaySnapshot() override;

  scoped_refptr<DrmDevice> drm() const { return drm_; }
  // Native properties of a display used by the DRM implementation in
  // configuring this display.
  uint32_t connector() const { return connector_; }
  uint32_t crtc() const { return crtc_; }
  drmModePropertyRes* dpms_property() const { return dpms_property_.get(); }

  // DisplaySnapshot overrides:
  std::string ToString() const override;

 private:
  scoped_refptr<DrmDevice> drm_;
  uint32_t connector_;
  uint32_t crtc_;
  ScopedDrmPropertyPtr dpms_property_;
  std::string name_;
  bool overscan_flag_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplaySnapshot);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_SNAPSHOT_H_
