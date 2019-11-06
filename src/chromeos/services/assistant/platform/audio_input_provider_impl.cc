// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_provider_impl.h"

#include "chromeos/services/assistant/public/features.h"

namespace chromeos {
namespace assistant {

AudioInputProviderImpl::AudioInputProviderImpl(
    service_manager::Connector* connector,
    const std::string& input_device_id,
    const std::string& hotword_device_id)
    : audio_input_(connector, input_device_id, hotword_device_id) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

AudioInputImpl& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  if (features::IsAudioEraserEnabled())
    return base::TimeTicks::Now().since_origin().InMicroseconds();

  return 0;
}

void AudioInputProviderImpl::SetMicState(bool mic_open) {
  audio_input_.SetMicState(mic_open);
}

void AudioInputProviderImpl::OnHotwordEnabled(bool enable) {
  audio_input_.OnHotwordEnabled(enable);
}

void AudioInputProviderImpl::SetDeviceId(const std::string& device_id) {
  audio_input_.SetDeviceId(device_id);
}

void AudioInputProviderImpl::SetHotwordDeviceId(const std::string& device_id) {
  audio_input_.SetHotwordDeviceId(device_id);
}

void AudioInputProviderImpl::SetDspHotwordLocale(std::string pref_locale) {
  audio_input_.SetDspHotwordLocale(pref_locale);
}

}  // namespace assistant
}  // namespace chromeos
