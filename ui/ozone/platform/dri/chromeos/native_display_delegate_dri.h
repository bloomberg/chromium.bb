// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_DRI_H_
#define UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_DRI_H_

#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/display/types/chromeos/native_display_delegate.h"

namespace ui {

class DisplaySnapshotDri;
class DriSurfaceFactory;

class NativeDisplayDelegateDri : public NativeDisplayDelegate {
 public:
  NativeDisplayDelegateDri(DriSurfaceFactory* surface_factory);
  virtual ~NativeDisplayDelegateDri();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32_t color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<DisplaySnapshot*> GetDisplays() OVERRIDE;
  virtual void AddMode(const DisplaySnapshot& output,
                       const DisplayMode* mode) OVERRIDE;
  virtual bool Configure(const DisplaySnapshot& output,
                         const DisplayMode* mode,
                         const gfx::Point& origin) OVERRIDE;
  virtual void CreateFrameBuffer(const gfx::Size& size) OVERRIDE;
  virtual bool GetHDCPState(const DisplaySnapshot& output,
                            HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state) OVERRIDE;
  virtual std::vector<ui::ColorCalibrationProfile>
      GetAvailableColorCalibrationProfiles(
          const ui::DisplaySnapshot& output) OVERRIDE;
  virtual bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) OVERRIDE;
  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE;

 private:
  DriSurfaceFactory* surface_factory_;  // Not owned.
  ScopedVector<const DisplayMode> cached_modes_;
  ScopedVector<DisplaySnapshotDri> cached_displays_;
  ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateDri);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_DRI_H_
