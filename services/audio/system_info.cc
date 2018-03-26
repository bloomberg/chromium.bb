// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/system_info.h"

#include "services/service_manager/public/cpp/service_context_ref.h"

namespace audio {

SystemInfo::SystemInfo(media::AudioManager* audio_manager)
    : helper_(audio_manager) {
  DETACH_FROM_SEQUENCE(binding_sequence_checker_);
}

SystemInfo::~SystemInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
}

void SystemInfo::Bind(
    mojom::SystemInfoRequest request,
    std::unique_ptr<service_manager::ServiceContextRef> context_ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
  bindings_.AddBinding(this, std::move(request), std::move(context_ref));
}

void SystemInfo::GetInputStreamParameters(
    const std::string& device_id,
    GetInputStreamParametersCallback callback) {
  helper_.GetInputStreamParameters(device_id, std::move(callback));
}

void SystemInfo::GetOutputStreamParameters(
    const std::string& device_id,
    GetOutputStreamParametersCallback callback) {
  helper_.GetOutputStreamParameters(device_id, std::move(callback));
}

void SystemInfo::HasInputDevices(HasInputDevicesCallback callback) {
  helper_.HasInputDevices(std::move(callback));
}

void SystemInfo::HasOutputDevices(HasOutputDevicesCallback callback) {
  helper_.HasOutputDevices(std::move(callback));
}

void SystemInfo::GetInputDeviceDescriptions(
    GetInputDeviceDescriptionsCallback callback) {
  helper_.GetDeviceDescriptions(true /* for_input */, std::move(callback));
}

void SystemInfo::GetOutputDeviceDescriptions(
    GetOutputDeviceDescriptionsCallback callback) {
  helper_.GetDeviceDescriptions(false /* for_input */, std::move(callback));
}

void SystemInfo::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    GetAssociatedOutputDeviceIDCallback callback) {
  helper_.GetAssociatedOutputDeviceID(input_device_id, std::move(callback));
}

void SystemInfo::GetInputDeviceInfo(const std::string& input_device_id,
                                    GetInputDeviceInfoCallback callback) {
  helper_.GetInputDeviceInfo(input_device_id, std::move(callback));
}

}  // namespace audio
