// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_STREAM_FACTORY_H_
#define SERVICES_AUDIO_STREAM_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/mojo/interfaces/audio_logging.mojom.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/audio/group_coordinator.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace base {
class UnguessableToken;
}

namespace media {
class AudioManager;
class AudioParameters;
class UserInputMonitor;
}  // namespace media

namespace service_manager {
class ServiceContextRef;
}

namespace audio {

class InputStream;
class LocalMuter;
class OutputStream;

// This class is used to provide the StreamFactory interface. It will typically
// be instantiated when needed and remain for the lifetime of the service.
// Destructing the factory will also destroy all the streams it has created.
// |audio_manager| must outlive the factory.
class StreamFactory final : public mojom::StreamFactory {
 public:
  explicit StreamFactory(media::AudioManager* audio_manager);
  ~StreamFactory() final;

  void Bind(mojom::StreamFactoryRequest request,
            std::unique_ptr<service_manager::ServiceContextRef> context_ref);

  // StreamFactory implementation.
  void CreateInputStream(media::mojom::AudioInputStreamRequest stream_request,
                         media::mojom::AudioInputStreamClientPtr client,
                         media::mojom::AudioInputStreamObserverPtr observer,
                         media::mojom::AudioLogPtr log,
                         const std::string& device_id,
                         const media::AudioParameters& params,
                         uint32_t shared_memory_count,
                         bool enable_agc,
                         CreateInputStreamCallback created_callback) final;

  void CreateOutputStream(
      media::mojom::AudioOutputStreamRequest stream_request,
      media::mojom::AudioOutputStreamClientPtr client,
      media::mojom::AudioOutputStreamObserverAssociatedPtrInfo observer_info,
      media::mojom::AudioLogPtr log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) final;
  void BindMuter(mojom::LocalMuterAssociatedRequest request,
                 const base::UnguessableToken& group_id) final;

 private:
  using InputStreamSet =
      base::flat_set<std::unique_ptr<InputStream>, base::UniquePtrComparator>;
  using OutputStreamSet =
      base::flat_set<std::unique_ptr<OutputStream>, base::UniquePtrComparator>;

  void DestroyInputStream(InputStream* stream);
  void DestroyOutputStream(OutputStream* stream);
  void DestroyMuter(LocalMuter* muter);

  SEQUENCE_CHECKER(owning_sequence_);

  media::AudioManager* const audio_manager_;
  media::UserInputMonitor* user_input_monitor_;

  mojo::BindingSet<mojom::StreamFactory,
                   std::unique_ptr<service_manager::ServiceContextRef>>
      bindings_;

  // Order of the following members is important for a clean shutdown.
  GroupCoordinator coordinator_;
  std::vector<std::unique_ptr<LocalMuter>> muters_;
  InputStreamSet input_streams_;
  OutputStreamSet output_streams_;

  DISALLOW_COPY_AND_ASSIGN(StreamFactory);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_STREAM_FACTORY_H_
