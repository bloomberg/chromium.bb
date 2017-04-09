// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IIRProcessor_h
#define IIRProcessor_h

#include "modules/webaudio/AudioNode.h"
#include "platform/audio/AudioDSPKernel.h"
#include "platform/audio/AudioDSPKernelProcessor.h"
#include "platform/audio/IIRFilter.h"
#include <memory>

namespace blink {

class IIRDSPKernel;

class IIRProcessor final : public AudioDSPKernelProcessor {
 public:
  IIRProcessor(float sample_rate,
               size_t number_of_channels,
               const Vector<double>& feedforward_coef,
               const Vector<double>& feedback_coef);
  ~IIRProcessor() override;

  std::unique_ptr<AudioDSPKernel> CreateKernel() override;

  void Process(const AudioBus* source,
               AudioBus* destination,
               size_t frames_to_process) override;

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

  AudioDoubleArray* Feedback() { return &feedback_; }
  AudioDoubleArray* Feedforward() { return &feedforward_; }

 private:
  // The feedback and feedforward filter coefficients for the IIR filter.
  AudioDoubleArray feedback_;
  AudioDoubleArray feedforward_;
  // This holds the IIR kernel for computing the frequency response.
  std::unique_ptr<IIRDSPKernel> response_kernel_;
};

}  // namespace blink

#endif  // IIRProcessor_h
