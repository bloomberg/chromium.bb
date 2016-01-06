// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_
#define UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/ozone_base_export.h"

namespace ui {

class OZONE_BASE_EXPORT NativeDisplayDelegateOzone
    : public NativeDisplayDelegate {
 public:
  NativeDisplayDelegateOzone();
  ~NativeDisplayDelegateOzone() override;

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  void TakeDisplayControl(const DisplayControlCallback& callback) override;
  void RelinquishDisplayControl(
      const DisplayControlCallback& callback) override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  void GetDisplays(const GetDisplaysCallback& callback) override;
  void AddMode(const ui::DisplaySnapshot& output,
               const ui::DisplayMode* mode) override;
  void Configure(const ui::DisplaySnapshot& output,
                 const ui::DisplayMode* mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback) override;
  void CreateFrameBuffer(const gfx::Size& size) override;
  void GetHDCPState(const ui::DisplaySnapshot& output,
                    const GetHDCPStateCallback& callback) override;
  void SetHDCPState(const ui::DisplaySnapshot& output,
                    ui::HDCPState state,
                    const SetHDCPStateCallback& callback) override;
  std::vector<ui::ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      const ui::DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) override;
  bool SetGammaRamp(const ui::DisplaySnapshot& output,
                    const std::vector<GammaRampRGBEntry>& lut) override;

  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;

 private:
  std::vector<scoped_ptr<DisplaySnapshot>> displays_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateOzone);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_
