/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioParam_h
#define AudioParam_h

#include <sys/types.h>
#include "base/memory/scoped_refptr.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/webaudio/AudioParamTimeline.h"
#include "modules/webaudio/AudioSummingJunction.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class AudioNodeOutput;

// Each AudioParam gets an identifier here.  This is mostly for instrospection
// if warnings or other messages need to be printed. It's useful to know what
// the AudioParam represents.  The name should include the node type and the
// name of the AudioParam.
enum AudioParamType {
  kParamTypeAudioBufferSourcePlaybackRate,
  kParamTypeAudioBufferSourceDetune,
  kParamTypeBiquadFilterFrequency,
  kParamTypeBiquadFilterQ,
  kParamTypeBiquadFilterGain,
  kParamTypeBiquadFilterDetune,
  kParamTypeDelayDelayTime,
  kParamTypeDynamicsCompressorThreshold,
  kParamTypeDynamicsCompressorKnee,
  kParamTypeDynamicsCompressorRatio,
  kParamTypeDynamicsCompressorAttack,
  kParamTypeDynamicsCompressorRelease,
  kParamTypeGainGain,
  kParamTypeOscillatorFrequency,
  kParamTypeOscillatorDetune,
  kParamTypeStereoPannerPan,
  kParamTypePannerPositionX,
  kParamTypePannerPositionY,
  kParamTypePannerPositionZ,
  kParamTypePannerOrientationX,
  kParamTypePannerOrientationY,
  kParamTypePannerOrientationZ,
  kParamTypeAudioListenerPositionX,
  kParamTypeAudioListenerPositionY,
  kParamTypeAudioListenerPositionZ,
  kParamTypeAudioListenerForwardX,
  kParamTypeAudioListenerForwardY,
  kParamTypeAudioListenerForwardZ,
  kParamTypeAudioListenerUpX,
  kParamTypeAudioListenerUpY,
  kParamTypeAudioListenerUpZ,
  kParamTypeConstantSourceOffset,
  kParamTypeAudioWorklet,
};

// AudioParamHandler is an actual implementation of web-exposed AudioParam
// interface. Each of AudioParam object creates and owns an AudioParamHandler,
// and it is responsible for all of AudioParam tasks. An AudioParamHandler
// object is owned by the originator AudioParam object, and some audio
// processing classes have additional references. An AudioParamHandler can
// outlive the owner AudioParam, and it never dies before the owner AudioParam
// dies.
class AudioParamHandler final : public ThreadSafeRefCounted<AudioParamHandler>,
                                public AudioSummingJunction {
 public:
  AudioParamType GetParamType() const { return param_type_; }
  void SetParamType(AudioParamType);
  // Return a nice name for the AudioParam.
  String GetParamName() const;

  static const double kDefaultSmoothingConstant;
  static const double kSnapThreshold;

  static scoped_refptr<AudioParamHandler> Create(BaseAudioContext& context,
                                                 AudioParamType param_type,
                                                 String param_name,
                                                 double default_value,
                                                 float min_value,
                                                 float max_value) {
    return base::AdoptRef(new AudioParamHandler(
        context, param_type, param_name, default_value, min_value, max_value));
  }

  // This should be used only in audio rendering thread.
  AudioDestinationHandler& DestinationHandler() const;

  // AudioSummingJunction
  void DidUpdate() override {}

  AudioParamTimeline& Timeline() { return timeline_; }

  // Intrinsic value.
  float Value();
  void SetValue(float);

  // Final value for k-rate parameters, otherwise use
  // calculateSampleAccurateValues() for a-rate.
  // Must be called in the audio thread.
  float FinalValue();

  float DefaultValue() const { return static_cast<float>(default_value_); }
  float MinValue() const { return min_value_; }
  float MaxValue() const { return max_value_; }

  // Value smoothing:

  // When a new value is set with setValue(), in our internal use of the
  // parameter we don't immediately jump to it.  Instead we smoothly approach
  // this value to avoid glitching.
  float SmoothedValue();

  // Smoothly exponentially approaches to (de-zippers) the desired value.
  // Returns true if smoothed value has already snapped exactly to value.
  bool Smooth();

  void ResetSmoothedValue() { timeline_.SetSmoothedValue(IntrinsicValue()); }

  bool HasSampleAccurateValues() {
    bool has_values =
        timeline_.HasValues(destination_handler_->CurrentSampleFrame(),
                            destination_handler_->SampleRate());

    return has_values || NumberOfRenderingConnections();
  }

  // Calculates numberOfValues parameter values starting at the context's
  // current time.
  // Must be called in the context's render thread.
  void CalculateSampleAccurateValues(float* values, unsigned number_of_values);

  // Connect an audio-rate signal to control this parameter.
  void Connect(AudioNodeOutput&);
  void Disconnect(AudioNodeOutput&);

  float IntrinsicValue() const { return NoBarrierLoad(&intrinsic_value_); }

  // TODO(crbug.com/764396): remove this when fixed.
  void WarnSetterOverlapsEvent(int event_index, BaseAudioContext&) const;

 private:
  AudioParamHandler(BaseAudioContext&,
                    AudioParamType,
                    String param_name,
                    double default_value,
                    float min,
                    float max);

  // sampleAccurate corresponds to a-rate (audio rate) vs. k-rate in the Web
  // Audio specification.
  void CalculateFinalValues(float* values,
                            unsigned number_of_values,
                            bool sample_accurate);
  void CalculateTimelineValues(float* values, unsigned number_of_values);

  int ComputeQHistogramValue(float) const;

  // The type of AudioParam, indicating what this AudioParam represents and what
  // node it belongs to.  Mostly for informational purposes and doesn't affect
  // implementation.
  AudioParamType param_type_;
  // Name of the AudioParam. This is only used for printing out more
  // informative warnings, and is otherwise arbitrary.
  String param_name_;

  // Intrinsic value
  float intrinsic_value_;
  void SetIntrinsicValue(float new_value);

  float default_value_;

  // Nominal range for the value
  float min_value_;
  float max_value_;

  AudioParamTimeline timeline_;

  // The destination node used to get necessary information like the smaple rate
  // and context time.
  scoped_refptr<AudioDestinationHandler> destination_handler_;
};

// AudioParam class represents web-exposed AudioParam interface.
class AudioParam final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioParam* Create(
      BaseAudioContext&,
      AudioParamType,
      String param_name,
      double default_value,
      float min_value = -std::numeric_limits<float>::max(),
      float max_value = std::numeric_limits<float>::max());

  void Trace(blink::Visitor*);
  // |handler| always returns a valid object.
  AudioParamHandler& Handler() const { return *handler_; }
  // |context| always returns a valid object.
  BaseAudioContext* Context() const { return context_; }

  AudioParamType GetParamType() const { return Handler().GetParamType(); }
  void SetParamType(AudioParamType);
  String GetParamName() const;

  float value() const;
  void setValue(float);
  float defaultValue() const;
  // Use when setting the initial value of an AudioParam when creating
  // the AudioParam.  This bypasses any deprecation messages about
  // using the value setter that has dezippering.
  //
  // TODO(rtoy): Replace all calls to this with just setValue() when
  // dezippering deprecation messages are removed.
  void setInitialValue(float);

  float minValue() const;
  float maxValue() const;

  AudioParam* setValueAtTime(float value, double time, ExceptionState&);
  AudioParam* linearRampToValueAtTime(float value,
                                      double time,
                                      ExceptionState&);
  AudioParam* exponentialRampToValueAtTime(float value,
                                           double time,
                                           ExceptionState&);
  AudioParam* setTargetAtTime(float target,
                              double time,
                              double time_constant,
                              ExceptionState&);
  AudioParam* setValueCurveAtTime(const Vector<float>& curve,
                                  double time,
                                  double duration,
                                  ExceptionState&);
  AudioParam* cancelScheduledValues(double start_time, ExceptionState&);
  AudioParam* cancelAndHoldAtTime(double start_time, ExceptionState&);

 private:
  AudioParam(BaseAudioContext&,
             AudioParamType,
             String param_name,
             double default_value,
             float min,
             float max);

  void WarnIfOutsideRange(const String& param_methd, float value);

  scoped_refptr<AudioParamHandler> handler_;
  Member<BaseAudioContext> context_;

  // TODO(crbug.com/764396): Remove this method and attribute when fixed.
  void WarnIfSetterOverlapsEvent();
  static bool s_value_setter_warning_done_;
};

}  // namespace blink

#endif  // AudioParam_h
