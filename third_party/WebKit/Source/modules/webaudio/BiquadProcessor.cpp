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

#include "modules/webaudio/BiquadProcessor.h"
#include <memory>
#include "modules/webaudio/BiquadDSPKernel.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

BiquadProcessor::BiquadProcessor(float sample_rate,
                                 size_t number_of_channels,
                                 AudioParamHandler& frequency,
                                 AudioParamHandler& q,
                                 AudioParamHandler& gain,
                                 AudioParamHandler& detune)
    : AudioDSPKernelProcessor(sample_rate, number_of_channels),
      type_(kLowPass),
      parameter1_(&frequency),
      parameter2_(&q),
      parameter3_(&gain),
      parameter4_(&detune),
      filter_coefficients_dirty_(true),
      has_sample_accurate_values_(false) {}

BiquadProcessor::~BiquadProcessor() {
  if (IsInitialized())
    Uninitialize();
}

std::unique_ptr<AudioDSPKernel> BiquadProcessor::CreateKernel() {
  return std::make_unique<BiquadDSPKernel>(this);
}

void BiquadProcessor::CheckForDirtyCoefficients() {
  // Deal with smoothing / de-zippering. Start out assuming filter parameters
  // are not changing.

  // The BiquadDSPKernel objects rely on this value to see if they need to
  // re-compute their internal filter coefficients.
  filter_coefficients_dirty_ = false;
  has_sample_accurate_values_ = false;

  if (parameter1_->HasSampleAccurateValues() ||
      parameter2_->HasSampleAccurateValues() ||
      parameter3_->HasSampleAccurateValues() ||
      parameter4_->HasSampleAccurateValues()) {
    filter_coefficients_dirty_ = true;
    has_sample_accurate_values_ = true;
  } else {
    if (has_just_reset_) {
      // Snap to exact values first time after reset, then smooth for subsequent
      // changes.
      parameter1_->ResetSmoothedValue();
      parameter2_->ResetSmoothedValue();
      parameter3_->ResetSmoothedValue();
      parameter4_->ResetSmoothedValue();
      filter_coefficients_dirty_ = true;
      has_just_reset_ = false;
    } else {
      // Smooth all of the filter parameters. If they haven't yet converged to
      // their target value then mark coefficients as dirty.
      bool is_stable1 = parameter1_->Smooth();
      bool is_stable2 = parameter2_->Smooth();
      bool is_stable3 = parameter3_->Smooth();
      bool is_stable4 = parameter4_->Smooth();
      if (!(is_stable1 && is_stable2 && is_stable3 && is_stable4))
        filter_coefficients_dirty_ = true;
    }
  }
}

void BiquadProcessor::Process(const AudioBus* source,
                              AudioBus* destination,
                              size_t frames_to_process) {
  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  // Synchronize with possible dynamic changes to the impulse response.
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
    // Can't get the lock. We must be in the middle of changing something.
    destination->Zero();
    return;
  }

  CheckForDirtyCoefficients();

  // For each channel of our input, process using the corresponding
  // BiquadDSPKernel into the output channel.
  for (unsigned i = 0; i < kernels_.size(); ++i)
    kernels_[i]->Process(source->Channel(i)->Data(),
                         destination->Channel(i)->MutableData(),
                         frames_to_process);
}

void BiquadProcessor::ProcessOnlyAudioParams(size_t frames_to_process) {
  DCHECK_LE(frames_to_process, AudioUtilities::kRenderQuantumFrames);

  float values[AudioUtilities::kRenderQuantumFrames];

  parameter1_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter2_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter3_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter4_->CalculateSampleAccurateValues(values, frames_to_process);
}

void BiquadProcessor::SetType(FilterType type) {
  if (type != type_) {
    type_ = type;
    Reset();  // The filter state must be reset only if the type has changed.
  }
}

void BiquadProcessor::GetFrequencyResponse(int n_frequencies,
                                           const float* frequency_hz,
                                           float* mag_response,
                                           float* phase_response) {
  // Compute the frequency response on a separate temporary kernel
  // to avoid interfering with the processing running in the audio
  // thread on the main kernels.

  std::unique_ptr<BiquadDSPKernel> response_kernel =
      std::make_unique<BiquadDSPKernel>(this);
  response_kernel->GetFrequencyResponse(n_frequencies, frequency_hz,
                                        mag_response, phase_response);
}

}  // namespace blink
