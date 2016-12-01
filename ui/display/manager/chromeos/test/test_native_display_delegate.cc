// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/test/test_native_display_delegate.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/manager/chromeos/test/action_logger.h"
#include "ui/display/types/display_mode.h"

namespace ui {
namespace test {

TestNativeDisplayDelegate::TestNativeDisplayDelegate(ActionLogger* log)
    : max_configurable_pixels_(0),
      get_hdcp_expectation_(true),
      set_hdcp_expectation_(true),
      hdcp_state_(HDCP_STATE_UNDESIRED),
      run_async_(false),
      log_(log) {}

TestNativeDisplayDelegate::~TestNativeDisplayDelegate() {}

void TestNativeDisplayDelegate::Initialize() {
  log_->AppendAction(kInitXRandR);
}

void TestNativeDisplayDelegate::GrabServer() {
  log_->AppendAction(kGrab);
}

void TestNativeDisplayDelegate::UngrabServer() {
  log_->AppendAction(kUngrab);
}

void TestNativeDisplayDelegate::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  log_->AppendAction(kTakeDisplayControl);
  callback.Run(true);
}

void TestNativeDisplayDelegate::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  log_->AppendAction(kRelinquishDisplayControl);
  callback.Run(true);
}

void TestNativeDisplayDelegate::SyncWithServer() {
  log_->AppendAction(kSync);
}

void TestNativeDisplayDelegate::SetBackgroundColor(uint32_t color_argb) {
  log_->AppendAction(GetBackgroundAction(color_argb));
}

void TestNativeDisplayDelegate::ForceDPMSOn() {
  log_->AppendAction(kForceDPMS);
}

void TestNativeDisplayDelegate::GetDisplays(
    const GetDisplaysCallback& callback) {
  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, outputs_));
  } else {
    callback.Run(outputs_);
  }
}

void TestNativeDisplayDelegate::AddMode(const DisplaySnapshot& output,
                                        const DisplayMode* mode) {
  log_->AppendAction(GetAddOutputModeAction(output, mode));
}

bool TestNativeDisplayDelegate::Configure(const DisplaySnapshot& output,
                                          const DisplayMode* mode,
                                          const gfx::Point& origin) {
  log_->AppendAction(GetCrtcAction(output, mode, origin));

  if (max_configurable_pixels_ == 0)
    return true;

  if (!mode)
    return false;

  return mode->size().GetArea() <= max_configurable_pixels_;
}

void TestNativeDisplayDelegate::Configure(const DisplaySnapshot& output,
                                          const DisplayMode* mode,
                                          const gfx::Point& origin,
                                          const ConfigureCallback& callback) {
  bool result = Configure(output, mode, origin);
  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, result));
  } else {
    callback.Run(result);
  }
}

void TestNativeDisplayDelegate::CreateFrameBuffer(const gfx::Size& size) {
  log_->AppendAction(
      GetFramebufferAction(size, outputs_.size() >= 1 ? outputs_[0] : nullptr,
                           outputs_.size() >= 2 ? outputs_[1] : nullptr));
}

void TestNativeDisplayDelegate::GetHDCPState(
    const DisplaySnapshot& output,
    const GetHDCPStateCallback& callback) {
  callback.Run(get_hdcp_expectation_, hdcp_state_);
}

void TestNativeDisplayDelegate::SetHDCPState(
    const DisplaySnapshot& output,
    HDCPState state,
    const SetHDCPStateCallback& callback) {
  log_->AppendAction(GetSetHDCPStateAction(output, state));
  callback.Run(set_hdcp_expectation_);
}

std::vector<ui::ColorCalibrationProfile>
TestNativeDisplayDelegate::GetAvailableColorCalibrationProfiles(
    const DisplaySnapshot& output) {
  return std::vector<ui::ColorCalibrationProfile>();
}

bool TestNativeDisplayDelegate::SetColorCalibrationProfile(
    const DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  return false;
}

bool TestNativeDisplayDelegate::SetColorCorrection(
    const ui::DisplaySnapshot& output,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  log_->AppendAction(SetColorCorrectionAction(output, degamma_lut, gamma_lut,
                                              correction_matrix));
  return true;
}

void TestNativeDisplayDelegate::AddObserver(NativeDisplayObserver* observer) {}

void TestNativeDisplayDelegate::RemoveObserver(
    NativeDisplayObserver* observer) {}

display::FakeDisplayController*
TestNativeDisplayDelegate::GetFakeDisplayController() {
  return nullptr;
}

}  // namespace test
}  // namespace ui
