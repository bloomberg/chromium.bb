// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_

#include <memory>

#include "base/macros.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"

namespace display {
class DisplaySnapshot;
}

namespace ui {

class GpuThreadAdapter;

class DrmDisplayHost : public GpuThreadObserver {
 public:
  DrmDisplayHost(GpuThreadAdapter* sender,
                 std::unique_ptr<display::DisplaySnapshot> params,
                 bool is_dummy);
  ~DrmDisplayHost() override;

  display::DisplaySnapshot* snapshot() const { return snapshot_.get(); }

  void UpdateDisplaySnapshot(std::unique_ptr<display::DisplaySnapshot> params);
  void Configure(const display::DisplayMode* mode,
                 const gfx::Point& origin,
                 display::ConfigureCallback callback);
  void GetHDCPState(display::GetHDCPStateCallback callback);
  void SetHDCPState(display::HDCPState state,
                    display::SetHDCPStateCallback callback);
  void SetColorMatrix(const std::vector<float>& color_matrix);
  void SetGammaCorrection(
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut);
  void SetPrivacyScreen(bool enabled);

  // Called when the IPC from the GPU process arrives to answer the above
  // commands.
  void OnDisplayConfigured(bool status);
  void OnHDCPStateReceived(bool status, display::HDCPState state);
  void OnHDCPStateUpdated(bool status);

  // GpuThreadObserver:
  void OnGpuProcessLaunched() override;
  void OnGpuThreadReady() override;
  void OnGpuThreadRetired() override;

 private:
  // Calls all the callbacks with failure.
  void ClearCallbacks();

  GpuThreadAdapter* const sender_;  // Not owned.

  std::unique_ptr<display::DisplaySnapshot> snapshot_;

  // Used during startup to signify that any display configuration should be
  // synchronous and succeed.
  bool is_dummy_;

  display::ConfigureCallback configure_callback_;
  display::GetHDCPStateCallback get_hdcp_callback_;
  display::SetHDCPStateCallback set_hdcp_callback_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
