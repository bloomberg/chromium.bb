// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_AUDIO_PLAYBACK_SINK_H_
#define REMOTING_CLIENT_AUDIO_AUDIO_PLAYBACK_SINK_H_

#include "base/macros.h"

namespace remoting {

class AsyncAudioDataSupplier;
struct AudioStreamFormat;

// This is an interface acting as the downstream of AsyncAudioDataSupplier.
class AudioPlaybackSink {
 public:
  AudioPlaybackSink() = default;
  virtual ~AudioPlaybackSink() = default;

  // Sets the data supplier to be used by the sink to request for more audio
  // data.
  // |supplier| must outlive |this|.
  virtual void SetDataSupplier(AsyncAudioDataSupplier* supplier) = 0;

  // Called whenever the stream format is first received or has been changed.
  virtual void ResetStreamFormat(const AudioStreamFormat& format) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioPlaybackSink);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_AUDIO_PLAYBACK_SINK_H_
