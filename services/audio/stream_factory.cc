// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/stream_factory.h"

#include <utility>

#include "services/audio/output_stream.h"

namespace audio {

StreamFactory::StreamFactory(media::AudioManager* audio_manager)
    : audio_manager_(audio_manager) {}

StreamFactory::~StreamFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

void StreamFactory::BindRequest(mojom::StreamFactoryRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  bindings_.AddBinding(this, std::move(request));
}

void StreamFactory::CreateOutputStream(
    media::mojom::AudioOutputStreamRequest stream_request,
    media::mojom::AudioOutputStreamClientPtr client,
    media::mojom::AudioOutputStreamObserverAssociatedPtrInfo observer_info,
    media::mojom::AudioLogPtr log,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  media::mojom::AudioOutputStreamObserverAssociatedPtr observer;
  observer.Bind(std::move(observer_info));

  // Unretained is safe since |this| indirectly owns the OutputStream.
  auto deleter_callback = base::BindOnce(&StreamFactory::RemoveOutputStream,
                                         base::Unretained(this));

  output_streams_.insert(std::make_unique<OutputStream>(
      std::move(created_callback), std::move(deleter_callback),
      std::move(stream_request), std::move(client), std::move(observer),
      std::move(log), audio_manager_, output_device_id, params, group_id));
}

void StreamFactory::RemoveOutputStream(OutputStream* stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  size_t erased = output_streams_.erase(stream);
  DCHECK_EQ(1u, erased);
}

}  // namespace audio
