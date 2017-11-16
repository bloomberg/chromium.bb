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

#ifndef WaveShaperProcessor_h
#define WaveShaperProcessor_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/webaudio/AudioNode.h"
#include "platform/audio/AudioDSPKernel.h"
#include "platform/audio/AudioDSPKernelProcessor.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace blink {

// WaveShaperProcessor is an AudioDSPKernelProcessor which uses
// WaveShaperDSPKernel objects to implement non-linear distortion effects.

class WaveShaperProcessor final : public AudioDSPKernelProcessor {
 public:
  enum OverSampleType { kOverSampleNone, kOverSample2x, kOverSample4x };

  WaveShaperProcessor(float sample_rate, size_t number_of_channels);

  ~WaveShaperProcessor() override;

  std::unique_ptr<AudioDSPKernel> CreateKernel() override;

  void Process(const AudioBus* source,
               AudioBus* destination,
               size_t frames_to_process) override;

  void SetCurve(const float* curve_data, unsigned curve_length);
  Vector<float>* Curve() const { return curve_.get(); };

  void SetOversample(OverSampleType);
  OverSampleType Oversample() const { return oversample_; }

 private:
  // m_curve represents the non-linear shaping curve.
  std::unique_ptr<Vector<float>> curve_;

  OverSampleType oversample_;
};

}  // namespace blink

#endif  // WaveShaperProcessor_h
