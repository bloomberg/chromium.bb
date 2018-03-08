// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_STREAM_FACTORY_H_
#define SERVICES_AUDIO_STREAM_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/mojo/interfaces/audio_logging.mojom.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace base {
class UnguessableToken;
}

namespace media {
class AudioManager;
class AudioParameters;
}  // namespace media

namespace audio {

class OutputStream;

// This class is used to provide the StreamFactory interface. It will typically
// be instantiated when needed and remain for the lifetime of the service.
// Destructing the factory will also destroy all the streams it has created.
// |audio_manager| must outlive the factory.
class StreamFactory final : public mojom::StreamFactory {
 public:
  explicit StreamFactory(media::AudioManager* audio_manager);
  ~StreamFactory() final;

  void BindRequest(mojom::StreamFactoryRequest request);

  // StreamFactory implementation.
  void CreateOutputStream(
      media::mojom::AudioOutputStreamRequest stream_request,
      media::mojom::AudioOutputStreamClientPtr client,
      media::mojom::AudioOutputStreamObserverAssociatedPtrInfo observer_info,
      media::mojom::AudioLogPtr log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) final;

 private:
  using OutputStreamSet =
      base::flat_set<std::unique_ptr<OutputStream>, base::UniquePtrComparator>;

  void RemoveOutputStream(OutputStream* stream);

  SEQUENCE_CHECKER(owning_sequence_);

  media::AudioManager* const audio_manager_;

  mojo::BindingSet<mojom::StreamFactory> bindings_;
  OutputStreamSet output_streams_;

  DISALLOW_COPY_AND_ASSIGN(StreamFactory);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_STREAM_FACTORY_H_
