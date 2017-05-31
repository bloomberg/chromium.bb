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

#include "modules/webaudio/BiquadDSPKernel.h"
#include <limits.h>
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/Vector.h"

namespace blink {

// FIXME: As a recursive linear filter, depending on its parameters, a biquad
// filter can have an infinite tailTime. In practice, Biquad filters do not
// usually (except for very high resonance values) have a tailTime of longer
// than approx. 200ms. This value could possibly be calculated based on the
// settings of the Biquad.
static const double kMaxBiquadDelayTime = 0.2;

void BiquadDSPKernel::UpdateCoefficientsIfNecessary(int frames_to_process) {
  if (GetBiquadProcessor()->FilterCoefficientsDirty()) {
    float cutoff_frequency[AudioUtilities::kRenderQuantumFrames];
    float q[AudioUtilities::kRenderQuantumFrames];
    float gain[AudioUtilities::kRenderQuantumFrames];
    float detune[AudioUtilities::kRenderQuantumFrames];  // in Cents

    SECURITY_CHECK(static_cast<unsigned>(frames_to_process) <=
                   AudioUtilities::kRenderQuantumFrames);

    if (GetBiquadProcessor()->HasSampleAccurateValues()) {
      GetBiquadProcessor()->Parameter1().CalculateSampleAccurateValues(
          cutoff_frequency, frames_to_process);
      GetBiquadProcessor()->Parameter2().CalculateSampleAccurateValues(
          q, frames_to_process);
      GetBiquadProcessor()->Parameter3().CalculateSampleAccurateValues(
          gain, frames_to_process);
      GetBiquadProcessor()->Parameter4().CalculateSampleAccurateValues(
          detune, frames_to_process);
      UpdateCoefficients(frames_to_process, cutoff_frequency, q, gain, detune);
    } else {
      cutoff_frequency[0] = GetBiquadProcessor()->Parameter1().SmoothedValue();
      q[0] = GetBiquadProcessor()->Parameter2().SmoothedValue();
      gain[0] = GetBiquadProcessor()->Parameter3().SmoothedValue();
      detune[0] = GetBiquadProcessor()->Parameter4().SmoothedValue();
      UpdateCoefficients(1, cutoff_frequency, q, gain, detune);
    }
  }
}

void BiquadDSPKernel::UpdateCoefficients(int number_of_frames,
                                         const float* cutoff_frequency,
                                         const float* q,
                                         const float* gain,
                                         const float* detune) {
  // Convert from Hertz to normalized frequency 0 -> 1.
  double nyquist = this->Nyquist();

  biquad_.SetHasSampleAccurateValues(number_of_frames > 1);

  for (int k = 0; k < number_of_frames; ++k) {
    double normalized_frequency = cutoff_frequency[k] / nyquist;

    // Offset frequency by detune.
    if (detune[k])
      normalized_frequency *= pow(2, detune[k] / 1200);

    // Configure the biquad with the new filter parameters for the appropriate
    // type of filter.
    switch (GetBiquadProcessor()->GetType()) {
      case BiquadProcessor::kLowPass:
        biquad_.SetLowpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kHighPass:
        biquad_.SetHighpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kBandPass:
        biquad_.SetBandpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kLowShelf:
        biquad_.SetLowShelfParams(k, normalized_frequency, gain[k]);
        break;

      case BiquadProcessor::kHighShelf:
        biquad_.SetHighShelfParams(k, normalized_frequency, gain[k]);
        break;

      case BiquadProcessor::kPeaking:
        biquad_.SetPeakingParams(k, normalized_frequency, q[k], gain[k]);
        break;

      case BiquadProcessor::kNotch:
        biquad_.SetNotchParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kAllpass:
        biquad_.SetAllpassParams(k, normalized_frequency, q[k]);
        break;
    }
  }
}

void BiquadDSPKernel::Process(const float* source,
                              float* destination,
                              size_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);
  DCHECK(GetBiquadProcessor());

  // Recompute filter coefficients if any of the parameters have changed.
  // FIXME: as an optimization, implement a way that a Biquad object can simply
  // copy its internal filter coefficients from another Biquad object.  Then
  // re-factor this code to only run for the first BiquadDSPKernel of each
  // BiquadProcessor.

  // The audio thread can't block on this lock; skip updating the coefficients
  // for this block if necessary. We'll get them the next time around.
  {
    MutexTryLocker try_locker(process_lock_);
    if (try_locker.Locked())
      UpdateCoefficientsIfNecessary(frames_to_process);
  }

  biquad_.Process(source, destination, frames_to_process);
}

void BiquadDSPKernel::GetFrequencyResponse(int n_frequencies,
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

  float cutoff_frequency;
  float q;
  float gain;
  float detune;  // in Cents

  {
    // Get a copy of the current biquad filter coefficients so we can update the
    // biquad with these values. We need to synchronize with process() to
    // prevent process() from updating the filter coefficients while we're
    // trying to access them. The process will update it next time around.
    //
    // The BiquadDSPKernel object here (along with it's Biquad object) is for
    // querying the frequency response and is NOT the same as the one in
    // process() which is used for performing the actual filtering. This one is
    // is created in BiquadProcessor::getFrequencyResponse for this purpose.
    // Both, however, point to the same BiquadProcessor object.
    //
    // FIXME: Simplify this: crbug.com/390266
    MutexLocker process_locker(process_lock_);

    cutoff_frequency = GetBiquadProcessor()->Parameter1().Value();
    q = GetBiquadProcessor()->Parameter2().Value();
    gain = GetBiquadProcessor()->Parameter3().Value();
    detune = GetBiquadProcessor()->Parameter4().Value();
  }

  UpdateCoefficients(1, &cutoff_frequency, &q, &gain, &detune);

  biquad_.GetFrequencyResponse(n_frequencies, frequency.data(), mag_response,
                               phase_response);
}

double BiquadDSPKernel::TailTime() const {
  return kMaxBiquadDelayTime;
}

double BiquadDSPKernel::LatencyTime() const {
  return 0;
}

}  // namespace blink
