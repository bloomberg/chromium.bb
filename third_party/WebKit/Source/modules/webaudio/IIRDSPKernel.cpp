// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRDSPKernel.h"

#include "platform/wtf/MathExtras.h"

namespace blink {

IIRDSPKernel::IIRDSPKernel(IIRProcessor* processor)
    : AudioDSPKernel(processor),
      iir_(processor->Feedforward(), processor->Feedback()) {
  tail_time_ = iir_.TailTime(processor->SampleRate());
}

void IIRDSPKernel::Process(const float* source,
                           float* destination,
                           size_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);

  iir_.Process(source, destination, frames_to_process);
}

void IIRDSPKernel::GetFrequencyResponse(int n_frequencies,
                                        const float* frequency_hz,
                                        float* mag_response,
                                        float* phase_response) {
  bool is_good =
      n_frequencies > 0 && frequency_hz && mag_response && phase_response;
  DCHECK(is_good);
  if (!is_good)
    return;

  Vector<float> frequency(n_frequencies);

  double nyquist = this->Nyquist();

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (int k = 0; k < n_frequencies; ++k)
    frequency[k] = frequency_hz[k] / nyquist;

  iir_.GetFrequencyResponse(n_frequencies, frequency.data(), mag_response,
                            phase_response);
}

double IIRDSPKernel::TailTime() const {
  return tail_time_;
}

double IIRDSPKernel::LatencyTime() const {
  return 0;
}

}  // namespace blink
