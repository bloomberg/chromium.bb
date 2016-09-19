// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake_display_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/display/util/display_util.h"

namespace display {

namespace {

// The EDID specification marks the top bit of the manufacturer id as reserved.
const uint16_t kReservedManufacturerID = 1 << 15;

// A random product name hash.
const uint32_t kProductCodeHash = base::Hash("Very Generic Display");

}  // namespace

FakeDisplayDelegate::FakeDisplayDelegate() {}

FakeDisplayDelegate::~FakeDisplayDelegate() {}

int64_t FakeDisplayDelegate::AddDisplay(const gfx::Size& display_size) {
  if (next_display_id_ == 0xFF) {
    LOG(ERROR) << "Exceeded display id limit";
    return display::Display::kInvalidDisplayID;
  }

  int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                 ++next_display_id_);
  // Add the first display as internal.
  ui::DisplayConnectionType type = displays_.empty()
                                       ? ui::DISPLAY_CONNECTION_TYPE_INTERNAL
                                       : ui::DISPLAY_CONNECTION_TYPE_UNKNOWN;
  std::string name = base::StringPrintf("Fake Display %d", next_display_id_);

  displays_.push_back(
      base::MakeUnique<FakeDisplaySnapshot>(id, display_size, type, name));
  OnConfigurationChanged();

  return id;
}

bool FakeDisplayDelegate::RemoveDisplay(int64_t display_id) {
  // Find display snapshot with matching id and remove it.
  for (auto iter = displays_.begin(); iter != displays_.end(); ++iter) {
    if ((*iter)->display_id() == display_id) {
      displays_.erase(iter);
      OnConfigurationChanged();
      return true;
    }
  }

  return false;
}

void FakeDisplayDelegate::Initialize() {
  DCHECK(!initialized_);
  InitFromCommandLine();

  // If no command line flags are provided then initialize a default display.
  if (displays_.empty())
    AddDisplay(gfx::Size(1024, 768));

  initialized_ = true;
}

void FakeDisplayDelegate::GrabServer() {}

void FakeDisplayDelegate::UngrabServer() {}

void FakeDisplayDelegate::TakeDisplayControl(
    const ui::DisplayControlCallback& callback) {
  callback.Run(false);
}

void FakeDisplayDelegate::RelinquishDisplayControl(
    const ui::DisplayControlCallback& callback) {
  callback.Run(false);
}

void FakeDisplayDelegate::SyncWithServer() {}

void FakeDisplayDelegate::SetBackgroundColor(uint32_t color_argb) {}

void FakeDisplayDelegate::ForceDPMSOn() {}

void FakeDisplayDelegate::GetDisplays(const ui::GetDisplaysCallback& callback) {
  std::vector<ui::DisplaySnapshot*> displays;
  for (auto& display : displays_)
    displays.push_back(display.get());
  callback.Run(displays);
}

void FakeDisplayDelegate::AddMode(const ui::DisplaySnapshot& output,
                                  const ui::DisplayMode* mode) {}

void FakeDisplayDelegate::Configure(const ui::DisplaySnapshot& output,
                                    const ui::DisplayMode* mode,
                                    const gfx::Point& origin,
                                    const ui::ConfigureCallback& callback) {
  callback.Run(true);
}

void FakeDisplayDelegate::CreateFrameBuffer(const gfx::Size& size) {}

void FakeDisplayDelegate::GetHDCPState(
    const ui::DisplaySnapshot& output,
    const ui::GetHDCPStateCallback& callback) {
  callback.Run(false, ui::HDCP_STATE_UNDESIRED);
}

void FakeDisplayDelegate::SetHDCPState(
    const ui::DisplaySnapshot& output,
    ui::HDCPState state,
    const ui::SetHDCPStateCallback& callback) {
  callback.Run(false);
}

std::vector<ui::ColorCalibrationProfile>
FakeDisplayDelegate::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  return std::vector<ui::ColorCalibrationProfile>();
}

bool FakeDisplayDelegate::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  return false;
}

bool FakeDisplayDelegate::SetColorCorrection(
    const ui::DisplaySnapshot& output,
    const std::vector<ui::GammaRampRGBEntry>& degamma_lut,
    const std::vector<ui::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  return false;
}

void FakeDisplayDelegate::AddObserver(ui::NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeDisplayDelegate::RemoveObserver(ui::NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

FakeDisplayController* FakeDisplayDelegate::GetFakeDisplayController() {
  return static_cast<FakeDisplayController*>(this);
}

void FakeDisplayDelegate::InitFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kScreenConfig))
    return;

  const std::string command_string =
      command_line->GetSwitchValueASCII(switches::kScreenConfig);

  // Split on commas and parse each display string.
  for (std::string part : base::SplitString(
           command_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int width = 0;
    int height = 0;
    if (sscanf(part.c_str(), "%dx%d", &width, &height) >= 2) {
      gfx::Size display_size(width, height);
      AddDisplay(display_size);
    } else {
      LOG(ERROR) << "Failed to parse display \"" << part << "\"";
    }
  }
}

void FakeDisplayDelegate::OnConfigurationChanged() {
  if (!initialized_)
    return;

  FOR_EACH_OBSERVER(ui::NativeDisplayObserver, observers_,
                    OnConfigurationChanged());
}

}  // namespace display
