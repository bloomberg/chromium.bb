// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake_display_delegate.h"

#include <string>
#include <utility>

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
  DCHECK(!display_size.IsEmpty());

  if (next_display_id_ == 0xFF) {
    LOG(ERROR) << "Exceeded display id limit";
    return Display::kInvalidDisplayID;
  }

  int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                 ++next_display_id_);

  FakeDisplaySnapshot::Builder builder;
  builder.SetId(id).SetNativeMode(display_size);
  builder.SetName(base::StringPrintf("Fake Display %d", next_display_id_));

  // Add the first display as internal.
  if (displays_.empty())
    builder.SetType(ui::DISPLAY_CONNECTION_TYPE_INTERNAL);

  return AddDisplay(builder.Build()) ? id : Display::kInvalidDisplayID;
}

bool FakeDisplayDelegate::AddDisplay(
    std::unique_ptr<ui::DisplaySnapshot> display) {
  DCHECK(display);

  int64_t display_id = display->display_id();
  // Check there is no existing display with the same id.
  for (auto& existing_display : displays_) {
    if (existing_display->display_id() == display_id) {
      LOG(ERROR) << "Display with id " << display_id << " already exists";
      return false;
    }
  }

  DVLOG(1) << "Added display " << display->ToString();
  displays_.push_back(std::move(display));
  OnConfigurationChanged();

  return true;
}

bool FakeDisplayDelegate::RemoveDisplay(int64_t display_id) {
  // Find display snapshot with matching id and remove it.
  for (auto iter = displays_.begin(); iter != displays_.end(); ++iter) {
    if ((*iter)->display_id() == display_id) {
      DVLOG(1) << "Removed display " << (*iter)->ToString();
      displays_.erase(iter);
      OnConfigurationChanged();
      return true;
    }
  }

  return false;
}

void FakeDisplayDelegate::Initialize() {
  DCHECK(!initialized_);

  // If no command line flags are provided then initialize a default display.
  if (!InitFromCommandLine())
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

bool FakeDisplayDelegate::InitFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kScreenConfig))
    return false;

  const std::string command_string =
      command_line->GetSwitchValueASCII(switches::kScreenConfig);

  // Start without any displays.
  if (command_string == "none")
    return true;

  // Split on commas and parse each display string.
  for (std::string part : base::SplitString(
           command_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::unique_ptr<ui::DisplaySnapshot> snapshot =
        CreateSnapshotFromSpec(part);
    if (snapshot) {
      AddDisplay(std::move(snapshot));
    } else {
      LOG(ERROR) << "Failed to parse display \"" << part << "\"";
    }
  }

  return true;
}

std::unique_ptr<ui::DisplaySnapshot>
FakeDisplayDelegate::CreateSnapshotFromSpec(const std::string& spec) {
  int width = 0;
  int height = 0;
  int dpi = 0;

  // Width and height are required but DPI is optional.
  int found = sscanf(spec.c_str(), "%dx%d^%d", &width, &height, &dpi);
  if (found < 2)
    return nullptr;

  int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                 ++next_display_id_);

  FakeDisplaySnapshot::Builder builder;
  builder.SetId(id).SetNativeMode(gfx::Size(width, height));
  builder.SetName(base::StringPrintf("Fake Display %d", next_display_id_));

  if (found >= 3)
    builder.SetDPI(dpi);

  // TODO(kylechar): Add type to the spec string.
  if (displays_.empty())
    builder.SetType(ui::DISPLAY_CONNECTION_TYPE_INTERNAL);

  return builder.Build();
}

void FakeDisplayDelegate::OnConfigurationChanged() {
  if (!initialized_)
    return;

  FOR_EACH_OBSERVER(ui::NativeDisplayObserver, observers_,
                    OnConfigurationChanged());
}

}  // namespace display
