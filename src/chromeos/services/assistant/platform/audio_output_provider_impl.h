// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/services/assistant/platform/audio_device_owner.h"
#include "chromeos/services/assistant/platform/volume_control_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class AssistantMediaSession;

class AudioOutputProviderImpl : public assistant_client::AudioOutputProvider {
 public:
  AudioOutputProviderImpl(
      service_manager::Connector* connector,
      AssistantMediaSession* media_session,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::string& device_id);
  ~AudioOutputProviderImpl() override;

  // assistant_client::AudioOutputProvider overrides:
  assistant_client::AudioOutput* CreateAudioOutput(
      assistant_client::OutputStreamType type,
      const assistant_client::OutputStreamFormat& stream_format) override;

  std::vector<assistant_client::OutputStreamEncoding>
  GetSupportedStreamEncodings() override;

  assistant_client::AudioInput* GetReferenceInput() override;

  bool SupportsPlaybackTimestamp() const override;

  assistant_client::VolumeControl& GetVolumeControl() override;

  void RegisterAudioEmittingStateCallback(
      AudioEmittingStateCallback callback) override;

 private:
  VolumeControlImpl volume_control_impl_;
  service_manager::Connector* connector_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  mojom::AssistantAudioDecoderFactoryPtr audio_decoder_factory_ptr_;
  mojom::AssistantAudioDecoderFactory* audio_decoder_factory_;
  std::string device_id_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_PROVIDER_IMPL_H_
