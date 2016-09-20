// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_FAKE_DISPLAY_DELEGATE_H_
#define UI_DISPLAY_FAKE_DISPLAY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/display_export.h"
#include "ui/display/fake_display_snapshot.h"
#include "ui/display/types/fake_display_controller.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

// A NativeDisplayDelegate implementation that manages fake displays. Fake
// displays mimic physical displays but do not actually exist. This is because
// Chrome OS does not take over the entire display when running off device and
// instead runs inside windows provided by the parent OS. Fake displays allow us
// to simulate different connected display states off device and to test display
// configuration and display management code.
//
// The size and number of displays can controlled via --screen-config=X
// command line flag with the format:
//   HxW[^dpi][,]
//     H: display height in pixels [int]
//     W: display width in pixels [int]
//     dpi: display physical size set based on DPI [int]
//
// Two 800x800 displays:
//  --screen-config=800x800,800x800
// One 1820x1080 display and one 400x400 display:
//  --screen-config=1920x1080,400x400
// No displays:
//  --screen-config=none
//
// FakeDisplayDelegate also implements FakeDisplayController which provides a
// way to change the display state at runtime.
class DISPLAY_EXPORT FakeDisplayDelegate : public ui::NativeDisplayDelegate,
                                           public FakeDisplayController {
 public:
  FakeDisplayDelegate();
  ~FakeDisplayDelegate() override;

  // FakeDisplayController:
  int64_t AddDisplay(const gfx::Size& display_size) override;
  bool AddDisplay(std::unique_ptr<ui::DisplaySnapshot> display) override;
  bool RemoveDisplay(int64_t display_id) override;

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  void TakeDisplayControl(const ui::DisplayControlCallback& callback) override;
  void RelinquishDisplayControl(
      const ui::DisplayControlCallback& callback) override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  void GetDisplays(const ui::GetDisplaysCallback& callback) override;
  void AddMode(const ui::DisplaySnapshot& output,
               const ui::DisplayMode* mode) override;
  void Configure(const ui::DisplaySnapshot& output,
                 const ui::DisplayMode* mode,
                 const gfx::Point& origin,
                 const ui::ConfigureCallback& callback) override;
  void CreateFrameBuffer(const gfx::Size& size) override;
  void GetHDCPState(const ui::DisplaySnapshot& output,
                    const ui::GetHDCPStateCallback& callback) override;
  void SetHDCPState(const ui::DisplaySnapshot& output,
                    ui::HDCPState state,
                    const ui::SetHDCPStateCallback& callback) override;
  std::vector<ui::ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      const ui::DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) override;
  bool SetColorCorrection(const ui::DisplaySnapshot& output,
                          const std::vector<ui::GammaRampRGBEntry>& degamma_lut,
                          const std::vector<ui::GammaRampRGBEntry>& gamma_lut,
                          const std::vector<float>& correction_matrix) override;
  void AddObserver(ui::NativeDisplayObserver* observer) override;
  void RemoveObserver(ui::NativeDisplayObserver* observer) override;
  FakeDisplayController* GetFakeDisplayController() override;

 protected:
  // Creates a display snapshot from the provided |spec| string. Return null if
  // |spec| is invalid.
  std::unique_ptr<ui::DisplaySnapshot> CreateSnapshotFromSpec(
      const std::string& spec);

  // Sets initial display snapshots from command line flag. Returns true if
  // command line flag was provided.
  bool InitFromCommandLine();

  // Updates observers when display configuration has changed. Will not update
  // until after |Initialize()| has been called.
  void OnConfigurationChanged();

 private:
  base::ObserverList<ui::NativeDisplayObserver> observers_;
  std::vector<std::unique_ptr<ui::DisplaySnapshot>> displays_;

  // If |Initialize()| has been called.
  bool initialized_ = false;

  // The next available display id.
  uint8_t next_display_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeDisplayDelegate);
};

}  // namespace display
#endif  // UI_DISPLAY_FAKE_DISPLAY_DELEGATE_H_
