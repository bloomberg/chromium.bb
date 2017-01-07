// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

class DrmDisplayHostManager;

class DrmNativeDisplayDelegate : public display::NativeDisplayDelegate {
 public:
  DrmNativeDisplayDelegate(DrmDisplayHostManager* display_manager);
  ~DrmNativeDisplayDelegate() override;

  void OnConfigurationChanged();
  void OnDisplaySnapshotsInvalidated();

  // display::NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  void TakeDisplayControl(
      const display::DisplayControlCallback& callback) override;
  void RelinquishDisplayControl(
      const display::DisplayControlCallback& callback) override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  void GetDisplays(const display::GetDisplaysCallback& callback) override;
  void AddMode(const display::DisplaySnapshot& output,
               const display::DisplayMode* mode) override;
  void Configure(const display::DisplaySnapshot& output,
                 const display::DisplayMode* mode,
                 const gfx::Point& origin,
                 const display::ConfigureCallback& callback) override;
  void CreateFrameBuffer(const gfx::Size& size) override;
  void GetHDCPState(const display::DisplaySnapshot& output,
                    const display::GetHDCPStateCallback& callback) override;
  void SetHDCPState(const display::DisplaySnapshot& output,
                    display::HDCPState state,
                    const display::SetHDCPStateCallback& callback) override;
  std::vector<display::ColorCalibrationProfile>
  GetAvailableColorCalibrationProfiles(
      const display::DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(
      const display::DisplaySnapshot& output,
      display::ColorCalibrationProfile new_profile) override;
  bool SetColorCorrection(
      const display::DisplaySnapshot& output,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut,
      const std::vector<float>& correction_matrix) override;

  void AddObserver(display::NativeDisplayObserver* observer) override;
  void RemoveObserver(display::NativeDisplayObserver* observer) override;
  display::FakeDisplayController* GetFakeDisplayController() override;

 private:
  DrmDisplayHostManager* display_manager_;  // Not owned.

  base::ObserverList<display::NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DrmNativeDisplayDelegate);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_
