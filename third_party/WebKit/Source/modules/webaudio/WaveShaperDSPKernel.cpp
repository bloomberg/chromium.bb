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

#include "modules/webaudio/WaveShaperDSPKernel.h"

#include <algorithm>
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Threading.h"

namespace blink {

WaveShaperDSPKernel::WaveShaperDSPKernel(WaveShaperProcessor* processor)
    : AudioDSPKernel(processor) {
  if (processor->Oversample() != WaveShaperProcessor::kOverSampleNone)
    LazyInitializeOversampling();
}

void WaveShaperDSPKernel::LazyInitializeOversampling() {
  if (!temp_buffer_) {
    temp_buffer_ = WTF::WrapUnique(
        new AudioFloatArray(AudioUtilities::kRenderQuantumFrames * 2));
    temp_buffer2_ = WTF::WrapUnique(
        new AudioFloatArray(AudioUtilities::kRenderQuantumFrames * 4));
    up_sampler_ =
        WTF::WrapUnique(new UpSampler(AudioUtilities::kRenderQuantumFrames));
    down_sampler_ = WTF::WrapUnique(
        new DownSampler(AudioUtilities::kRenderQuantumFrames * 2));
    up_sampler2_ = WTF::WrapUnique(
        new UpSampler(AudioUtilities::kRenderQuantumFrames * 2));
    down_sampler2_ = WTF::WrapUnique(
        new DownSampler(AudioUtilities::kRenderQuantumFrames * 4));
  }
}

void WaveShaperDSPKernel::Process(const float* source,
                                  float* destination,
                                  size_t frames_to_process) {
  switch (GetWaveShaperProcessor()->Oversample()) {
    case WaveShaperProcessor::kOverSampleNone:
      ProcessCurve(source, destination, frames_to_process);
      break;
    case WaveShaperProcessor::kOverSample2x:
      ProcessCurve2x(source, destination, frames_to_process);
      break;
    case WaveShaperProcessor::kOverSample4x:
      ProcessCurve4x(source, destination, frames_to_process);
      break;

    default:
      NOTREACHED();
  }
}

void WaveShaperDSPKernel::ProcessCurve(const float* source,
                                       float* destination,
                                       size_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);
  DCHECK(GetWaveShaperProcessor());

  Vector<float>* curve = GetWaveShaperProcessor()->Curve();
  if (!curve) {
    // Act as "straight wire" pass-through if no curve is set.
    memcpy(destination, source, sizeof(float) * frames_to_process);
    return;
  }

  float* curve_data = curve->data();
  int curve_length = curve->size();

  DCHECK(curve_data);

  if (!curve_data || !curve_length) {
    memcpy(destination, source, sizeof(float) * frames_to_process);
    return;
  }

  // Apply waveshaping curve.
  for (unsigned i = 0; i < frames_to_process; ++i) {
    const float input = source[i];

    // Calculate a virtual index based on input -1 -> +1 with -1 being curve[0],
    // +1 being curve[curveLength - 1], and 0 being at the center of the curve
    // data. Then linearly interpolate between the two points in the curve.
    double virtual_index = 0.5 * (input + 1) * (curve_length - 1);
    double output;

    if (virtual_index < 0) {
      // input < -1, so use curve[0]
      output = curve_data[0];
    } else if (virtual_index >= curve_length - 1) {
      // input >= 1, so use last curve value
      output = curve_data[curve_length - 1];
    } else {
      // The general case where -1 <= input < 1, where 0 <= virtualIndex <
      // curveLength - 1, so interpolate between the nearest samples on the
      // curve.
      unsigned index1 = static_cast<unsigned>(virtual_index);
      unsigned index2 = index1 + 1;
      double interpolation_factor = virtual_index - index1;

      double value1 = curve_data[index1];
      double value2 = curve_data[index2];

      output =
          (1.0 - interpolation_factor) * value1 + interpolation_factor * value2;
    }
    destination[i] = output;
  }
}

void WaveShaperDSPKernel::ProcessCurve2x(const float* source,
                                         float* destination,
                                         size_t frames_to_process) {
  bool is_safe = frames_to_process == AudioUtilities::kRenderQuantumFrames;
  DCHECK(is_safe);
  if (!is_safe)
    return;

  float* temp_p = temp_buffer_->Data();

  up_sampler_->Process(source, temp_p, frames_to_process);

  // Process at 2x up-sampled rate.
  ProcessCurve(temp_p, temp_p, frames_to_process * 2);

  down_sampler_->Process(temp_p, destination, frames_to_process * 2);
}

void WaveShaperDSPKernel::ProcessCurve4x(const float* source,
                                         float* destination,
                                         size_t frames_to_process) {
  bool is_safe = frames_to_process == AudioUtilities::kRenderQuantumFrames;
  DCHECK(is_safe);
  if (!is_safe)
    return;

  float* temp_p = temp_buffer_->Data();
  float* temp_p2 = temp_buffer2_->Data();

  up_sampler_->Process(source, temp_p, frames_to_process);
  up_sampler2_->Process(temp_p, temp_p2, frames_to_process * 2);

  // Process at 4x up-sampled rate.
  ProcessCurve(temp_p2, temp_p2, frames_to_process * 4);

  down_sampler2_->Process(temp_p2, temp_p, frames_to_process * 4);
  down_sampler_->Process(temp_p, destination, frames_to_process * 2);
}

void WaveShaperDSPKernel::Reset() {
  if (up_sampler_) {
    up_sampler_->Reset();
    down_sampler_->Reset();
    up_sampler2_->Reset();
    down_sampler2_->Reset();
  }
}

double WaveShaperDSPKernel::LatencyTime() const {
  size_t latency_frames = 0;
  WaveShaperDSPKernel* kernel = const_cast<WaveShaperDSPKernel*>(this);

  switch (kernel->GetWaveShaperProcessor()->Oversample()) {
    case WaveShaperProcessor::kOverSampleNone:
      break;
    case WaveShaperProcessor::kOverSample2x:
      latency_frames += up_sampler_->LatencyFrames();
      latency_frames += down_sampler_->LatencyFrames();
      break;
    case WaveShaperProcessor::kOverSample4x: {
      // Account for first stage upsampling.
      latency_frames += up_sampler_->LatencyFrames();
      latency_frames += down_sampler_->LatencyFrames();

      // Account for second stage upsampling.
      // and divide by 2 to get back down to the regular sample-rate.
      size_t latency_frames2 =
          (up_sampler2_->LatencyFrames() + down_sampler2_->LatencyFrames()) / 2;
      latency_frames += latency_frames2;
      break;
    }
    default:
      NOTREACHED();
  }

  return static_cast<double>(latency_frames) / SampleRate();
}

}  // namespace blink
