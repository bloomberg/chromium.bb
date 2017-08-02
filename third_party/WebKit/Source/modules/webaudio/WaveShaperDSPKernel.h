/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#ifndef WaveShaperDSPKernel_h
#define WaveShaperDSPKernel_h

#include "modules/webaudio/WaveShaperProcessor.h"
#include "platform/audio/AudioArray.h"
#include "platform/audio/AudioDSPKernel.h"
#include "platform/audio/DownSampler.h"
#include "platform/audio/UpSampler.h"
#include <memory>

namespace blink {

class WaveShaperProcessor;

// WaveShaperDSPKernel is an AudioDSPKernel and is responsible for non-linear
// distortion on one channel.

class WaveShaperDSPKernel final : public AudioDSPKernel {
 public:
  explicit WaveShaperDSPKernel(WaveShaperProcessor*);

  // AudioDSPKernel
  void Process(const float* source,
               float* dest,
               size_t frames_to_process) override;
  void Reset() override;
  double TailTime() const override { return 0; }
  double LatencyTime() const override;

  // Oversampling requires more resources, so let's only allocate them if
  // needed.
  void LazyInitializeOversampling();

 protected:
  // Apply the shaping curve.
  void ProcessCurve(const float* source, float* dest, size_t frames_to_process);

  // Use up-sampling, process at the higher sample-rate, then down-sample.
  void ProcessCurve2x(const float* source,
                      float* dest,
                      size_t frames_to_process);
  void ProcessCurve4x(const float* source,
                      float* dest,
                      size_t frames_to_process);

  WaveShaperProcessor* GetWaveShaperProcessor() {
    return static_cast<WaveShaperProcessor*>(Processor());
  }

  // Oversampling.
  std::unique_ptr<AudioFloatArray> temp_buffer_;
  std::unique_ptr<AudioFloatArray> temp_buffer2_;
  std::unique_ptr<UpSampler> up_sampler_;
  std::unique_ptr<DownSampler> down_sampler_;
  std::unique_ptr<UpSampler> up_sampler2_;
  std::unique_ptr<DownSampler> down_sampler2_;
};

}  // namespace blink

#endif  // WaveShaperDSPKernel_h
