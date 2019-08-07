// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_factory.h"

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chromeos {
namespace assistant {

AssistantAudioDecoderFactory::AssistantAudioDecoderFactory(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

AssistantAudioDecoderFactory::~AssistantAudioDecoderFactory() = default;

void AssistantAudioDecoderFactory::CreateAssistantAudioDecoder(
    mojom::AssistantAudioDecoderRequest request,
    mojom::AssistantAudioDecoderClientPtr client,
    mojom::AssistantMediaDataSourcePtr data_source) {
  mojo::MakeStrongBinding(
      std::make_unique<AssistantAudioDecoder>(
          service_ref_->Clone(), std::move(client), std::move(data_source)),
      std::move(request));
}

}  // namespace assistant
}  // namespace chromeos
