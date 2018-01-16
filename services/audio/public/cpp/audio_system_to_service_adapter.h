// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_AUDIO_SYSTEM_TO_SERVICE_ADAPTER_H_
#define SERVICES_AUDIO_PUBLIC_CPP_AUDIO_SYSTEM_TO_SERVICE_ADAPTER_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "media/audio/audio_system.h"
#include "services/audio/public/interfaces/system_info.mojom.h"

namespace service_manager {
class Connector;
}

namespace audio {

// Provides media::AudioSystem implementation on top of Audio service.
// In case connection to Audio service is lost, reply callbacks will run with
// empty optionals / false booleans.
class AudioSystemToServiceAdapter : public media::AudioSystem {
 public:
  explicit AudioSystemToServiceAdapter(
      std::unique_ptr<service_manager::Connector> connector);

  ~AudioSystemToServiceAdapter() override;

  // AudioSystem implementation.
  void GetInputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_callback) override;
  void GetOutputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_callback) override;
  void HasInputDevices(OnBoolCallback on_has_devices_callback) override;
  void HasOutputDevices(OnBoolCallback on_has_devices_callback) override;
  void GetDeviceDescriptions(
      bool for_input,
      OnDeviceDescriptionsCallback on_descriptions_cp) override;
  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      OnDeviceIdCallback on_device_id_callback) override;
  void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_callback) override;

 private:
  mojom::SystemInfo* GetSystemInfo();
  void OnConnectionError();

  // Will be bound to the thread AudioSystemToServiceAdapter is used on.
  const std::unique_ptr<service_manager::Connector> connector_;
  mojom::SystemInfoPtr system_info_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(AudioSystemToServiceAdapter);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_AUDIO_SYSTEM_TO_SERVICE_ADAPTER_H_
