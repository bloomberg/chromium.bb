// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IIRDSPKernel_h
#define IIRDSPKernel_h

#include "modules/webaudio/IIRProcessor.h"
#include "platform/audio/AudioDSPKernel.h"
#include "platform/audio/IIRFilter.h"

namespace blink {

class IIRProcessor;

class IIRDSPKernel final : public AudioDSPKernel {
 public:
  explicit IIRDSPKernel(IIRProcessor*);

  // AudioDSPKernel
  void Process(const float* source,
               float* dest,
               size_t frames_to_process) override;
  void Reset() override { iir_.Reset(); }

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

  double TailTime() const override;
  double LatencyTime() const override;

 protected:
  IIRFilter iir_;

 private:
  double tail_time_;
};

}  // namespace blink

#endif  // IIRDSPKernel_h
