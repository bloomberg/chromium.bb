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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_OSCILLATOR_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_OSCILLATOR_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_oscillator_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/audio_scheduled_source_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class OscillatorOptions;
class PeriodicWave;
class PeriodicWaveImpl;

// OscillatorNode is an audio generator of periodic waveforms.

class OscillatorHandler final : public AudioScheduledSourceHandler {
 public:
  // The waveform type.
  // These must be defined as in the .idl file.
  enum : uint8_t {
    SINE = 0,
    SQUARE = 1,
    SAWTOOTH = 2,
    TRIANGLE = 3,
    CUSTOM = 4
  };

  // Breakpoints where we deicde to do linear interoplation, 3-point
  // interpolation or 5-point interpolation.  See DoInterpolation().
  static constexpr float kInterpolate2Point = 0.3;
  static constexpr float kInterpolate3Point = 0.16;

  static scoped_refptr<OscillatorHandler> Create(AudioNode&,
                                                 float sample_rate,
                                                 const String& oscillator_type,
                                                 PeriodicWaveImpl* wave_table,
                                                 AudioParamHandler& frequency,
                                                 AudioParamHandler& detune);
  ~OscillatorHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;

  String GetType() const;
  void SetType(const String&, ExceptionState&);

  void SetPeriodicWave(PeriodicWaveImpl*);

  void HandleStoppableSourceNode() override;

 private:
  OscillatorHandler(AudioNode&,
                    float sample_rate,
                    const String& oscillator_type,
                    PeriodicWaveImpl* wave_table,
                    AudioParamHandler& frequency,
                    AudioParamHandler& detune);
  bool SetType(uint8_t);  // Returns true on success.

  // Returns true if there are sample-accurate timeline parameter changes.
  bool CalculateSampleAccuratePhaseIncrements(uint32_t frames_to_process);

  bool PropagatesSilence() const override;

  // Compute the output for k-rate AudioParams
  double ProcessKRate(int n, float* dest_p, double virtual_read_index) const;

  // Scalar version for the main loop in ProcessKRate().  Returns the updated
  // virtual_read_index.
  double ProcessKRateScalar(int start_index,
                            int n,
                            float* dest_p,
                            double virtual_read_index,
                            float frequency,
                            float rate_scale) const;

  // Vectorized version (if available) for the main loop in ProcessKRate().
  // Returns the number of elements processed and the updated
  // virtual_read_index.
  std::tuple<int, double> ProcessKRateVector(int n,
                                             float* dest_p,
                                             double virtual_read_index,
                                             float frequency,
                                             float rate_scale) const;

  // Compute the output for a-rate AudioParams
  double ProcessARate(int n,
                      float* dest_p,
                      double virtual_read_index,
                      float* phase_increments) const;

  // Scalar version of ProcessARate().  Also handles any values not handled by
  // the vector version.
  //
  //   k
  //     start index for where to write the result (and read phase_increments)
  //   n
  //     total number of frames to process
  //   destination
  //     Array where the samples values are written
  //   virtual_read_index
  //     index into the wave data tables containing the waveform
  //   phase_increments
  //     phase change to use for each frame of output
  //
  // Returns the updated virtual_read_index.
  double ProcessARateScalar(int k,
                            int n,
                            float* destination,
                            double virtual_read_index,
                            const float* phase_increments) const;

  // Vector version of ProcessARate().  Returns the number of frames processed
  // and the update virtual_read_index.
  std::tuple<int, double> ProcessARateVector(
      int n,
      float* destination,
      double virtual_read_index,
      const float* phase_increments) const;

  // Handles the linear interpolation in ProcessARateVector().
  //
  //   destination
  //     Where the interpolated values are written.
  //   virtual_read_index
  //     index into the wave table data
  //   phase_increments
  //     phase increments array
  //   periodic_wave_size
  //     Length of the periodic wave stored in the wave tables
  //   lower_wave_data
  //     Array of the 4 lower wave table arrays
  //   higher_wave_data
  //     Array of the 4 higher wave table arrays
  //   table_interpolation_factor
  //     Array of linear interpolation factors to use between the lower and
  //     higher wave tables.
  //
  // Returns the updated virtual_read_index
  double ProcessARateVectorKernel(
      float* destination,
      double virtual_read_index,
      const float* phase_increments,
      unsigned periodic_wave_size,
      const float* const lower_wave_data[4],
      const float* const higher_wave_data[4],
      const float table_interpolation_factor[4]) const;

  // One of the waveform types defined in the enum.
  uint8_t type_;

  // Frequency value in Hertz.
  scoped_refptr<AudioParamHandler> frequency_;

  // Detune value (deviating from the frequency) in Cents.
  scoped_refptr<AudioParamHandler> detune_;

  bool first_render_;

  // m_virtualReadIndex is a sample-frame index into our buffer representing the
  // current playback position.  Since it's floating-point, it has sub-sample
  // accuracy.
  double virtual_read_index_;

  // Stores sample-accurate values calculated according to frequency and detune.
  AudioFloatArray phase_increments_;
  AudioFloatArray detune_values_;

  // PeriodicWaveImpl cannot cause cycles with OscillatorNode as it is not
  // scriptable.
  CrossThreadPersistent<PeriodicWaveImpl> periodic_wave_;
};

class OscillatorNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OscillatorNode* Create(BaseAudioContext&,
                                const String& oscillator_type,
                                PeriodicWave* wave_table,
                                ExceptionState&);
  static OscillatorNode* Create(BaseAudioContext*,
                                const OscillatorOptions*,
                                ExceptionState&);

  OscillatorNode(BaseAudioContext&,
                 const String& oscillator_type,
                 PeriodicWave* wave_table);
  void Trace(Visitor*) const override;

  String type() const;
  void setType(const String&, ExceptionState&);
  AudioParam* frequency();
  AudioParam* detune();
  void setPeriodicWave(PeriodicWave*);

  OscillatorHandler& GetOscillatorHandler() const;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  Member<AudioParam> frequency_;
  Member<AudioParam> detune_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_OSCILLATOR_NODE_H_
