// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_INPUT_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_INPUT_STREAM_BROKER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/interfaces/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace media {
class UserInputMonitorBase;
}

namespace content {

// AudioInputStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It is operated on the UI thread.
class CONTENT_EXPORT AudioInputStreamBroker final
    : public AudioStreamBroker,
      public media::mojom::AudioInputStreamObserver {
 public:
  AudioInputStreamBroker(
      int render_process_id,
      int render_frame_id,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      audio::mojom::AudioProcessingConfigPtr processing_config,
      AudioStreamBroker::DeleterCallback deleter,
      mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client);

  ~AudioInputStreamBroker() final;

  // Creates the stream.
  void CreateStream(audio::mojom::StreamFactory* factory) final;

  // media::AudioInputStreamObserver implementation.
  void DidStartRecording() final;

 private:
  void StreamCreated(media::mojom::AudioInputStreamPtr stream,
                     media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
                     bool initially_muted,
                     const base::Optional<base::UnguessableToken>& stream_id);

  void ObserverBindingLost(uint32_t reason, const std::string& description);
  void ClientBindingLost();
  void Cleanup();

  const std::string device_id_;
  media::AudioParameters params_;
  const uint32_t shared_memory_count_;
  const bool enable_agc_;
  media::UserInputMonitorBase* user_input_monitor_ = nullptr;

  // Indicates that CreateStream has been called, but not StreamCreated.
  bool awaiting_created_ = false;

  DeleterCallback deleter_;

  audio::mojom::AudioProcessingConfigPtr processing_config_;
  mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client_;
  mojo::Binding<AudioInputStreamObserver> observer_binding_;
  media::mojom::AudioInputStreamClientRequest client_request_;

  media::mojom::AudioInputStreamObserver::DisconnectReason disconnect_reason_ =
      media::mojom::AudioInputStreamObserver::DisconnectReason::
          kDocumentDestroyed;

  base::WeakPtrFactory<AudioInputStreamBroker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputStreamBroker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_INPUT_STREAM_BROKER_H_
