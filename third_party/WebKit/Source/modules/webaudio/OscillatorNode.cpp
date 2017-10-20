/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "modules/webaudio/OscillatorNode.h"
#include <algorithm>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/PeriodicWave.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/VectorMath.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

using namespace VectorMath;

OscillatorHandler::OscillatorHandler(AudioNode& node,
                                     float sample_rate,
                                     const String& oscillator_type,
                                     PeriodicWave* wave_table,
                                     AudioParamHandler& frequency,
                                     AudioParamHandler& detune)
    : AudioScheduledSourceHandler(kNodeTypeOscillator, node, sample_rate),
      frequency_(&frequency),
      detune_(&detune),
      first_render_(true),
      virtual_read_index_(0),
      phase_increments_(AudioUtilities::kRenderQuantumFrames),
      detune_values_(AudioUtilities::kRenderQuantumFrames) {
  if (wave_table) {
    // A PeriodicWave overrides any value for the oscillator type,
    // forcing the type to be 'custom".
    SetPeriodicWave(wave_table);
  } else {
    if (oscillator_type == "sine")
      SetType(SINE);
    else if (oscillator_type == "square")
      SetType(SQUARE);
    else if (oscillator_type == "sawtooth")
      SetType(SAWTOOTH);
    else if (oscillator_type == "triangle")
      SetType(TRIANGLE);
    else
      NOTREACHED();
  }

  // An oscillator is always mono.
  AddOutput(1);

  Initialize();
}

scoped_refptr<OscillatorHandler> OscillatorHandler::Create(
    AudioNode& node,
    float sample_rate,
    const String& oscillator_type,
    PeriodicWave* wave_table,
    AudioParamHandler& frequency,
    AudioParamHandler& detune) {
  return WTF::AdoptRef(new OscillatorHandler(node, sample_rate, oscillator_type,
                                             wave_table, frequency, detune));
}

OscillatorHandler::~OscillatorHandler() {
  Uninitialize();
}

String OscillatorHandler::GetType() const {
  switch (type_) {
    case SINE:
      return "sine";
    case SQUARE:
      return "square";
    case SAWTOOTH:
      return "sawtooth";
    case TRIANGLE:
      return "triangle";
    case CUSTOM:
      return "custom";
    default:
      NOTREACHED();
      return "custom";
  }
}

void OscillatorHandler::SetType(const String& type,
                                ExceptionState& exception_state) {
  if (type == "sine") {
    SetType(SINE);
  } else if (type == "square") {
    SetType(SQUARE);
  } else if (type == "sawtooth") {
    SetType(SAWTOOTH);
  } else if (type == "triangle") {
    SetType(TRIANGLE);
  } else if (type == "custom") {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "'type' cannot be set directly to "
                                      "'custom'.  Use setPeriodicWave() to "
                                      "create a custom Oscillator type.");
  }
}

bool OscillatorHandler::SetType(unsigned type) {
  PeriodicWave* periodic_wave = nullptr;

  switch (type) {
    case SINE:
      periodic_wave = Context()->GetPeriodicWave(SINE);
      break;
    case SQUARE:
      periodic_wave = Context()->GetPeriodicWave(SQUARE);
      break;
    case SAWTOOTH:
      periodic_wave = Context()->GetPeriodicWave(SAWTOOTH);
      break;
    case TRIANGLE:
      periodic_wave = Context()->GetPeriodicWave(TRIANGLE);
      break;
    case CUSTOM:
    default:
      // Return false for invalid types, including CUSTOM since
      // setPeriodicWave() method must be called explicitly.
      NOTREACHED();
      return false;
  }

  SetPeriodicWave(periodic_wave);
  type_ = type;
  return true;
}

bool OscillatorHandler::CalculateSampleAccuratePhaseIncrements(
    size_t frames_to_process) {
  bool is_good = frames_to_process <= phase_increments_.size() &&
                 frames_to_process <= detune_values_.size();
  DCHECK(is_good);
  if (!is_good)
    return false;

  if (first_render_) {
    first_render_ = false;
    frequency_->ResetSmoothedValue();
    detune_->ResetSmoothedValue();
  }

  bool has_sample_accurate_values = false;
  bool has_frequency_changes = false;
  float* phase_increments = phase_increments_.Data();

  float final_scale = periodic_wave_->RateScale();

  if (frequency_->HasSampleAccurateValues()) {
    has_sample_accurate_values = true;
    has_frequency_changes = true;

    // Get the sample-accurate frequency values and convert to phase increments.
    // They will be converted to phase increments below.
    frequency_->CalculateSampleAccurateValues(phase_increments,
                                              frames_to_process);
  } else {
    // Handle ordinary parameter smoothing/de-zippering if there are no
    // scheduled changes.
    frequency_->Smooth();
    float frequency = frequency_->SmoothedValue();
    final_scale *= frequency;
  }

  if (detune_->HasSampleAccurateValues()) {
    has_sample_accurate_values = true;

    // Get the sample-accurate detune values.
    float* detune_values =
        has_frequency_changes ? detune_values_.Data() : phase_increments;
    detune_->CalculateSampleAccurateValues(detune_values, frames_to_process);

    // Convert from cents to rate scalar.
    float k = 1.0 / 1200;
    Vsmul(detune_values, 1, &k, detune_values, 1, frames_to_process);
    for (unsigned i = 0; i < frames_to_process; ++i)
      detune_values[i] = powf(
          2, detune_values[i]);  // FIXME: converting to expf() will be faster.

    if (has_frequency_changes) {
      // Multiply frequencies by detune scalings.
      Vmul(detune_values, 1, phase_increments, 1, phase_increments, 1,
           frames_to_process);
    }
  } else {
    // Handle ordinary parameter smoothing/de-zippering if there are no
    // scheduled changes.
    detune_->Smooth();
    float detune = detune_->SmoothedValue();
    float detune_scale = powf(2, detune / 1200);
    final_scale *= detune_scale;
  }

  if (has_sample_accurate_values) {
    // Convert from frequency to wavetable increment.
    Vsmul(phase_increments, 1, &final_scale, phase_increments, 1,
          frames_to_process);
  }

  return has_sample_accurate_values;
}

void OscillatorHandler::Process(size_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized() || !output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  DCHECK_LE(frames_to_process, phase_increments_.size());
  if (frames_to_process > phase_increments_.size())
    return;

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
    // Too bad - the tryLock() failed. We must be in the middle of changing
    // wave-tables.
    output_bus->Zero();
    return;
  }

  // We must access m_periodicWave only inside the lock.
  if (!periodic_wave_.Get()) {
    output_bus->Zero();
    return;
  }

  size_t quantum_frame_offset;
  size_t non_silent_frames_to_process;
  double start_frame_offset;

  UpdateSchedulingInfo(frames_to_process, output_bus, quantum_frame_offset,
                       non_silent_frames_to_process, start_frame_offset);

  if (!non_silent_frames_to_process) {
    output_bus->Zero();
    return;
  }

  unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;

  float* dest_p = output_bus->Channel(0)->MutableData();

  DCHECK_LE(quantum_frame_offset, frames_to_process);

  // We keep virtualReadIndex double-precision since we're accumulating values.
  double virtual_read_index = virtual_read_index_;

  float rate_scale = periodic_wave_->RateScale();
  float inv_rate_scale = 1 / rate_scale;
  bool has_sample_accurate_values =
      CalculateSampleAccuratePhaseIncrements(frames_to_process);

  float frequency = 0;
  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  if (!has_sample_accurate_values) {
    frequency = frequency_->SmoothedValue();
    float detune = detune_->SmoothedValue();
    float detune_scale = powf(2, detune / 1200);
    frequency *= detune_scale;
    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);
  }

  float incr = frequency * rate_scale;
  float* phase_increments = phase_increments_.Data();

  unsigned read_index_mask = periodic_wave_size - 1;

  // Start rendering at the correct offset.
  dest_p += quantum_frame_offset;
  int n = non_silent_frames_to_process;

  // If startFrameOffset is not 0, that means the oscillator doesn't actually
  // start at quantumFrameOffset, but just past that time.  Adjust destP and n
  // to reflect that, and adjust virtualReadIndex to start the value at
  // startFrameOffset.
  if (start_frame_offset > 0) {
    ++dest_p;
    --n;
    virtual_read_index += (1 - start_frame_offset) * frequency * rate_scale;
    DCHECK(virtual_read_index < periodic_wave_size);
  } else if (start_frame_offset < 0) {
    virtual_read_index = -start_frame_offset * frequency * rate_scale;
  }

  while (n--) {
    unsigned read_index = static_cast<unsigned>(virtual_read_index);
    unsigned read_index2 = read_index + 1;

    // Contain within valid range.
    read_index = read_index & read_index_mask;
    read_index2 = read_index2 & read_index_mask;

    if (has_sample_accurate_values) {
      incr = *phase_increments++;

      frequency = inv_rate_scale * incr;
      periodic_wave_->WaveDataForFundamentalFrequency(
          frequency, lower_wave_data, higher_wave_data,
          table_interpolation_factor);
    }

    float sample1_lower = lower_wave_data[read_index];
    float sample2_lower = lower_wave_data[read_index2];
    float sample1_higher = higher_wave_data[read_index];
    float sample2_higher = higher_wave_data[read_index2];

    // Linearly interpolate within each table (lower and higher).
    float interpolation_factor =
        static_cast<float>(virtual_read_index) - read_index;
    float sample_higher = (1 - interpolation_factor) * sample1_higher +
                          interpolation_factor * sample2_higher;
    float sample_lower = (1 - interpolation_factor) * sample1_lower +
                         interpolation_factor * sample2_lower;

    // Then interpolate between the two tables.
    float sample = (1 - table_interpolation_factor) * sample_higher +
                   table_interpolation_factor * sample_lower;

    *dest_p++ = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  virtual_read_index_ = virtual_read_index;

  output_bus->ClearSilentFlag();
}

void OscillatorHandler::SetPeriodicWave(PeriodicWave* periodic_wave) {
  DCHECK(IsMainThread());
  DCHECK(periodic_wave);

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  periodic_wave_ = periodic_wave;
  type_ = CUSTOM;
}

bool OscillatorHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished() || !periodic_wave_.Get();
}

// ----------------------------------------------------------------

OscillatorNode::OscillatorNode(BaseAudioContext& context,
                               const String& oscillator_type,
                               PeriodicWave* wave_table)
    : AudioScheduledSourceNode(context),
      // Use musical pitch standard A440 as a default.
      frequency_(AudioParam::Create(context,
                                    kParamTypeOscillatorFrequency,
                                    440,
                                    -context.sampleRate() / 2,
                                    context.sampleRate() / 2)),
      // Default to no detuning.
      detune_(AudioParam::Create(context, kParamTypeOscillatorDetune, 0)) {
  SetHandler(OscillatorHandler::Create(
      *this, context.sampleRate(), oscillator_type, wave_table,
      frequency_->Handler(), detune_->Handler()));
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext& context,
                                       const String& oscillator_type,
                                       PeriodicWave* wave_table,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  return new OscillatorNode(context, oscillator_type, wave_table);
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext* context,
                                       const OscillatorOptions& options,
                                       ExceptionState& exception_state) {
  if (options.type() == "custom" && !options.hasPeriodicWave()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "A PeriodicWave must be specified if the type is set to \"custom\"");
    return nullptr;
  }

  OscillatorNode* node =
      Create(*context, options.type(), options.periodicWave(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->detune()->setInitialValue(options.detune());
  node->frequency()->setInitialValue(options.frequency());

  return node;
}

void OscillatorNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(frequency_);
  visitor->Trace(detune_);
  AudioScheduledSourceNode::Trace(visitor);
}

OscillatorHandler& OscillatorNode::GetOscillatorHandler() const {
  return static_cast<OscillatorHandler&>(Handler());
}

String OscillatorNode::type() const {
  return GetOscillatorHandler().GetType();
}

void OscillatorNode::setType(const String& type,
                             ExceptionState& exception_state) {
  GetOscillatorHandler().SetType(type, exception_state);
}

AudioParam* OscillatorNode::frequency() {
  return frequency_;
}

AudioParam* OscillatorNode::detune() {
  return detune_;
}

void OscillatorNode::setPeriodicWave(PeriodicWave* wave) {
  GetOscillatorHandler().SetPeriodicWave(wave);
}

}  // namespace blink
