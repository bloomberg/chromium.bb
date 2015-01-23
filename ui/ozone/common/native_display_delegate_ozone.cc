// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/native_display_delegate_ozone.h"

#include "base/logging.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/common/display_util.h"

namespace ui {

NativeDisplayDelegateOzone::NativeDisplayDelegateOzone() {
}

NativeDisplayDelegateOzone::~NativeDisplayDelegateOzone() {
}

void NativeDisplayDelegateOzone::Initialize() {
  DisplaySnapshot_Params params;
  if (CreateSnapshotFromCommandLine(&params)) {
    DCHECK_NE(DISPLAY_CONNECTION_TYPE_NONE, params.type);
    displays_.push_back(new DisplaySnapshotProxy(params));
  }
}

void NativeDisplayDelegateOzone::GrabServer() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::UngrabServer() {
  NOTIMPLEMENTED();
}

bool NativeDisplayDelegateOzone::TakeDisplayControl() {
  NOTIMPLEMENTED();
  return false;
}

bool NativeDisplayDelegateOzone::RelinquishDisplayControl() {
  NOTIMPLEMENTED();
  return false;
}

void NativeDisplayDelegateOzone::SyncWithServer() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::SetBackgroundColor(uint32_t color_argb) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::ForceDPMSOn() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::GetDisplays(
    const GetDisplaysCallback& callback) {
  callback.Run(displays_.get());
}

void NativeDisplayDelegateOzone::AddMode(const ui::DisplaySnapshot& output,
                                         const ui::DisplayMode* mode) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::Configure(const ui::DisplaySnapshot& output,
                                           const ui::DisplayMode* mode,
                                           const gfx::Point& origin,
                                           const ConfigureCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(true);
}

void NativeDisplayDelegateOzone::CreateFrameBuffer(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool NativeDisplayDelegateOzone::GetHDCPState(const ui::DisplaySnapshot& output,
                                              ui::HDCPState* state) {
  NOTIMPLEMENTED();
  return false;
}

bool NativeDisplayDelegateOzone::SetHDCPState(const ui::DisplaySnapshot& output,
                                              ui::HDCPState state) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<ui::ColorCalibrationProfile>
NativeDisplayDelegateOzone::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  NOTIMPLEMENTED();
  return std::vector<ui::ColorCalibrationProfile>();
}

bool NativeDisplayDelegateOzone::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  NOTIMPLEMENTED();
  return false;
}

void NativeDisplayDelegateOzone::AddObserver(NativeDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::RemoveObserver(
    NativeDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace ui
