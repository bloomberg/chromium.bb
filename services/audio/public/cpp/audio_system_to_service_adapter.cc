// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_system_to_service_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace audio {

AudioSystemToServiceAdapter::AudioSystemToServiceAdapter(
    std::unique_ptr<service_manager::Connector> connector)
    : connector_(std::move(connector)) {
  DCHECK(connector_);
  DETACH_FROM_THREAD(thread_checker_);
}

AudioSystemToServiceAdapter::~AudioSystemToServiceAdapter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void AudioSystemToServiceAdapter::GetInputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  GetSystemInfo()->GetInputStreamParameters(
      device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                     std::move(on_params_callback), base::nullopt));
}

void AudioSystemToServiceAdapter::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  GetSystemInfo()->GetOutputStreamParameters(
      device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                     std::move(on_params_callback), base::nullopt));
}

void AudioSystemToServiceAdapter::HasInputDevices(
    OnBoolCallback on_has_devices_callback) {
  GetSystemInfo()->HasInputDevices(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(on_has_devices_callback), false));
}

void AudioSystemToServiceAdapter::HasOutputDevices(
    OnBoolCallback on_has_devices_callback) {
  GetSystemInfo()->HasOutputDevices(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(on_has_devices_callback), false));
}

void AudioSystemToServiceAdapter::GetDeviceDescriptions(
    bool for_input,
    OnDeviceDescriptionsCallback on_descriptions_callback) {
  auto reply_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(on_descriptions_callback), media::AudioDeviceDescriptions());
  if (for_input)
    GetSystemInfo()->GetInputDeviceDescriptions(std::move(reply_callback));
  else
    GetSystemInfo()->GetOutputDeviceDescriptions(std::move(reply_callback));
}

void AudioSystemToServiceAdapter::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_callback) {
  GetSystemInfo()->GetAssociatedOutputDeviceID(
      input_device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                           std::move(on_device_id_callback), base::nullopt));
}

void AudioSystemToServiceAdapter::GetInputDeviceInfo(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_callback) {
  GetSystemInfo()->GetInputDeviceInfo(
      input_device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                           std::move(on_input_device_info_callback),
                           base::nullopt, base::nullopt));
}

mojom::SystemInfo* AudioSystemToServiceAdapter::GetSystemInfo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!system_info_) {
    DVLOG(4) << "AudioSystemToServiceAdapter::GetSystemInfo: SystemInfo bind "
                "request";
    connector_->BindInterface(mojom::kServiceName,
                              mojo::MakeRequest(&system_info_));
    system_info_.set_connection_error_handler(
        base::BindOnce(&AudioSystemToServiceAdapter::OnConnectionError,
                       base::Unretained(this)));
    DCHECK(system_info_);
  }
  return system_info_.get();
}

void AudioSystemToServiceAdapter::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(4) << "AudioSystemToServiceAdapter::OnConnectionError";
  system_info_.reset();
}

}  // namespace audio
