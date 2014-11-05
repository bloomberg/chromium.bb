// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_
#define UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_

#include "base/macros.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

class NativeDisplayDelegateOzone : public NativeDisplayDelegate {
 public:
  NativeDisplayDelegateOzone();
  virtual ~NativeDisplayDelegateOzone();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() override;
  virtual void GrabServer() override;
  virtual void UngrabServer() override;
  virtual bool TakeDisplayControl() override;
  virtual bool RelinquishDisplayControl() override;
  virtual void SyncWithServer() override;
  virtual void SetBackgroundColor(uint32_t color_argb) override;
  virtual void ForceDPMSOn() override;
  virtual std::vector<ui::DisplaySnapshot*> GetDisplays() override;
  virtual void AddMode(const ui::DisplaySnapshot& output,
                       const ui::DisplayMode* mode) override;
  virtual bool Configure(const ui::DisplaySnapshot& output,
                         const ui::DisplayMode* mode,
                         const gfx::Point& origin) override;
  virtual void CreateFrameBuffer(const gfx::Size& size) override;
  virtual bool GetHDCPState(const ui::DisplaySnapshot& output,
                            ui::HDCPState* state) override;
  virtual bool SetHDCPState(const ui::DisplaySnapshot& output,
                            ui::HDCPState state) override;
  virtual std::vector<ui::ColorCalibrationProfile>
  GetAvailableColorCalibrationProfiles(
      const ui::DisplaySnapshot& output) override;
  virtual bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) override;
  virtual void AddObserver(NativeDisplayObserver* observer) override;
  virtual void RemoveObserver(NativeDisplayObserver* observer) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateOzone);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_DISPLAY_DELEGATE_OZONE_H_
