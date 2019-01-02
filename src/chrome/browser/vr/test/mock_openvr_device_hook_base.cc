// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"

MockOpenVRBase::MockOpenVRBase() {
  device::OpenVRDeviceProvider::SetTestHook(this);
}

MockOpenVRBase::~MockOpenVRBase() {
  device::OpenVRDeviceProvider::SetTestHook(nullptr);
}

void MockOpenVRBase::OnFrameSubmitted(device::SubmittedFrameData frame_data) {}

device::DeviceConfig MockOpenVRBase::WaitGetDeviceConfig() {
  device::DeviceConfig ret = {0.1f /* ipd*/,
                              {1, 1, 1, 1} /* raw projection left */,
                              {1, 1, 1, 1} /* raw projection right */};
  return ret;
}

device::PoseFrameData MockOpenVRBase::WaitGetPresentingPose() {
  device::PoseFrameData pose = {};
  pose.is_valid = true;
  // Identity matrix.
  pose.device_to_origin[0] = 1;
  pose.device_to_origin[5] = 1;
  pose.device_to_origin[10] = 1;
  pose.device_to_origin[15] = 1;
  return pose;
}

device::PoseFrameData MockOpenVRBase::WaitGetMagicWindowPose() {
  device::PoseFrameData pose = {};
  pose.is_valid = true;
  // Identity matrix.
  pose.device_to_origin[0] = 1;
  pose.device_to_origin[5] = 1;
  pose.device_to_origin[10] = 1;
  pose.device_to_origin[15] = 1;
  return pose;
}
