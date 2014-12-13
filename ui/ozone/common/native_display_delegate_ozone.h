// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_
#define UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

class NativeDisplayDelegateOzone : public NativeDisplayDelegate {
 public:
  NativeDisplayDelegateOzone();
  ~NativeDisplayDelegateOzone() override;

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  bool TakeDisplayControl() override;
  bool RelinquishDisplayControl() override;
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
  bool GetHDCPState(const ui::DisplaySnapshot& output,
                    ui::HDCPState* state) override;
  bool SetHDCPState(const ui::DisplaySnapshot& output,
                    ui::HDCPState state) override;
  std::vector<ui::ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      const ui::DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) override;
  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;

 private:
  ScopedVector<DisplaySnapshot> displays_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateOzone);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_
