// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_

#include <limits>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"

namespace chromecast {
namespace media {

// This class is used to decrypt then proxy audio data to an external
// CmaBackend::AudioDecoder over gRPC.
class MultizoneAudioDecoderProxy : public CmaBackend::AudioDecoder {
 public:
  using BufferStatus = CmaBackend::Decoder::BufferStatus;
  using Delegate = CmaBackend::Decoder::Delegate;
  using Observer = CmaBackend::AudioDecoder::Observer;
  using RenderingDelay = CmaBackend::AudioDecoder::RenderingDelay;
  using Statistics = CmaBackend::AudioDecoder::Statistics;

  ~MultizoneAudioDecoderProxy() override = default;

  virtual bool Initialize() = 0;
  virtual bool Start(int64_t start_pts) = 0;
  virtual void Stop() = 0;
  virtual bool Pause() = 0;
  virtual bool Resume() = 0;
  virtual bool SetPlaybackRate(float rate) = 0;
  virtual void LogicalPause() = 0;
  virtual void LogicalResume() = 0;
  virtual int64_t GetCurrentPts() const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_
