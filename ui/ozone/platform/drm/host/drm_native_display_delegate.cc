// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"

#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"

namespace ui {

DrmNativeDisplayDelegate::DrmNativeDisplayDelegate(
    DrmDisplayHostManager* display_manager)
    : display_manager_(display_manager) {
}

DrmNativeDisplayDelegate::~DrmNativeDisplayDelegate() {
  display_manager_->RemoveDelegate(this);
}

void DrmNativeDisplayDelegate::OnConfigurationChanged() {
  FOR_EACH_OBSERVER(NativeDisplayObserver, observers_,
                    OnConfigurationChanged());
}

void DrmNativeDisplayDelegate::Initialize() {
  display_manager_->AddDelegate(this);
}

void DrmNativeDisplayDelegate::GrabServer() {
}

void DrmNativeDisplayDelegate::UngrabServer() {
}

bool DrmNativeDisplayDelegate::TakeDisplayControl() {
  return display_manager_->TakeDisplayControl();
}

bool DrmNativeDisplayDelegate::RelinquishDisplayControl() {
  return display_manager_->RelinquishDisplayControl();
}

void DrmNativeDisplayDelegate::SyncWithServer() {
}

void DrmNativeDisplayDelegate::SetBackgroundColor(uint32_t color_argb) {
}

void DrmNativeDisplayDelegate::ForceDPMSOn() {
}

void DrmNativeDisplayDelegate::GetDisplays(
    const GetDisplaysCallback& callback) {
  display_manager_->UpdateDisplays(callback);
}

void DrmNativeDisplayDelegate::AddMode(const ui::DisplaySnapshot& output,
                                       const ui::DisplayMode* mode) {
}

void DrmNativeDisplayDelegate::Configure(const ui::DisplaySnapshot& output,
                                         const ui::DisplayMode* mode,
                                         const gfx::Point& origin,
                                         const ConfigureCallback& callback) {
  display_manager_->Configure(output.display_id(), mode, origin, callback);
}

void DrmNativeDisplayDelegate::CreateFrameBuffer(const gfx::Size& size) {
}

void DrmNativeDisplayDelegate::GetHDCPState(
    const ui::DisplaySnapshot& output,
    const GetHDCPStateCallback& callback) {
  display_manager_->GetHDCPState(output.display_id(), callback);
}

void DrmNativeDisplayDelegate::SetHDCPState(
    const ui::DisplaySnapshot& output,
    ui::HDCPState state,
    const SetHDCPStateCallback& callback) {
  display_manager_->SetHDCPState(output.display_id(), state, callback);
}

std::vector<ui::ColorCalibrationProfile>
DrmNativeDisplayDelegate::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  return std::vector<ui::ColorCalibrationProfile>();
}

bool DrmNativeDisplayDelegate::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  NOTIMPLEMENTED();
  return false;
}

bool DrmNativeDisplayDelegate::SetGammaRamp(
    const ui::DisplaySnapshot& output,
    const std::vector<GammaRampRGBEntry>& lut) {
  return display_manager_->SetGammaRamp(output.display_id(), lut);
}

void DrmNativeDisplayDelegate::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DrmNativeDisplayDelegate::RemoveObserver(NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
