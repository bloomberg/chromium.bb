/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef AudioResampler_h
#define AudioResampler_h

#include <memory>
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioResamplerKernel.h"
#include "platform/audio/AudioSourceProvider.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

// AudioResampler resamples the audio stream from an AudioSourceProvider.
// The audio stream may be single or multi-channel.
// The default constructor defaults to single-channel (mono).

class PLATFORM_EXPORT AudioResampler {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(AudioResampler);

 public:
  AudioResampler();
  AudioResampler(unsigned number_of_channels);
  ~AudioResampler() = default;

  // Given an AudioSourceProvider, process() resamples the source stream into
  // destinationBus.
  void Process(AudioSourceProvider*,
               AudioBus* destination_bus,
               size_t frames_to_process);

  // Resets the processing state.
  void Reset();

  void ConfigureChannels(unsigned number_of_channels);

  // 0 < rate <= MaxRate
  void SetRate(double rate);
  double Rate() const { return rate_; }

  static const double kMaxRate;

 private:
  double rate_;
  Vector<std::unique_ptr<AudioResamplerKernel>> kernels_;
  scoped_refptr<AudioBus> source_bus_;
};

}  // namespace blink

#endif  // AudioResampler_h
