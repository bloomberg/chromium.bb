// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_service.h"

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_factory.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chromeos {
namespace assistant {

namespace {

void OnAudioDecoderFactoryRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    mojom::AssistantAudioDecoderFactoryRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<AssistantAudioDecoderFactory>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

AssistantAudioDecoderService::AssistantAudioDecoderService() = default;

AssistantAudioDecoderService::~AssistantAudioDecoderService() = default;

std::unique_ptr<service_manager::Service>
AssistantAudioDecoderService::CreateService() {
  return std::make_unique<AssistantAudioDecoderService>();
}

void AssistantAudioDecoderService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      context()->CreateQuitClosure());
  registry_.AddInterface(
      base::BindRepeating(&OnAudioDecoderFactoryRequest, ref_factory_.get()),
      base::ThreadTaskRunnerHandle::Get());
}

void AssistantAudioDecoderService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace assistant
}  // namespace chromeos
