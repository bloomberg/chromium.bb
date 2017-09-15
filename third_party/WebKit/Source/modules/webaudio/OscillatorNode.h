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

#ifndef OscillatorNode_h
#define OscillatorNode_h

#include "modules/webaudio/AudioParam.h"
#include "modules/webaudio/AudioScheduledSourceNode.h"
#include "modules/webaudio/OscillatorOptions.h"
#include "platform/audio/AudioBus.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Threading.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class OscillatorOptions;
class PeriodicWave;

// OscillatorNode is an audio generator of periodic waveforms.

class OscillatorHandler final : public AudioScheduledSourceHandler {
 public:
  // The waveform type.
  // These must be defined as in the .idl file.
  enum { SINE = 0, SQUARE = 1, SAWTOOTH = 2, TRIANGLE = 3, CUSTOM = 4 };

  static RefPtr<OscillatorHandler> Create(AudioNode&,
                                          float sample_rate,
                                          const String& oscillator_type,
                                          PeriodicWave* wave_table,
                                          AudioParamHandler& frequency,
                                          AudioParamHandler& detune);
  ~OscillatorHandler() override;

  // AudioHandler
  void Process(size_t frames_to_process) override;

  String GetType() const;
  void SetType(const String&, ExceptionState&);

  void SetPeriodicWave(PeriodicWave*);

 private:
  OscillatorHandler(AudioNode&,
                    float sample_rate,
                    const String& oscillator_type,
                    PeriodicWave* wave_table,
                    AudioParamHandler& frequency,
                    AudioParamHandler& detune);
  bool SetType(unsigned);  // Returns true on success.

  // Returns true if there are sample-accurate timeline parameter changes.
  bool CalculateSampleAccuratePhaseIncrements(size_t frames_to_process);

  bool PropagatesSilence() const override;

  // One of the waveform types defined in the enum.
  unsigned short type_;

  // Frequency value in Hertz.
  RefPtr<AudioParamHandler> frequency_;

  // Detune value (deviating from the frequency) in Cents.
  RefPtr<AudioParamHandler> detune_;

  bool first_render_;

  // m_virtualReadIndex is a sample-frame index into our buffer representing the
  // current playback position.  Since it's floating-point, it has sub-sample
  // accuracy.
  double virtual_read_index_;

  // Stores sample-accurate values calculated according to frequency and detune.
  AudioFloatArray phase_increments_;
  AudioFloatArray detune_values_;

  // This Persistent doesn't make a reference cycle including the owner
  // OscillatorNode. It is cross-thread, as it will be accessed by the audio
  // thread.
  CrossThreadPersistent<PeriodicWave> periodic_wave_;
};

class OscillatorNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OscillatorNode* Create(BaseAudioContext&,
                                const String& oscillator_type,
                                PeriodicWave* wave_table,
                                ExceptionState&);
  static OscillatorNode* Create(BaseAudioContext*,
                                const OscillatorOptions&,
                                ExceptionState&);
  DECLARE_VIRTUAL_TRACE();

  String type() const;
  void setType(const String&, ExceptionState&);
  AudioParam* frequency();
  AudioParam* detune();
  void setPeriodicWave(PeriodicWave*);

 private:
  OscillatorNode(BaseAudioContext&,
                 const String& oscillator_type,
                 PeriodicWave* wave_table);
  OscillatorHandler& GetOscillatorHandler() const;

  Member<AudioParam> frequency_;
  Member<AudioParam> detune_;
};

}  // namespace blink

#endif  // OscillatorNode_h
