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

#include "modules/webaudio/AudioParam.h"

#include "core/dom/ExceptionCode.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "platform/Histogram.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

const double AudioParamHandler::kDefaultSmoothingConstant = 0.05;
const double AudioParamHandler::kSnapThreshold = 0.001;

AudioParamHandler::AudioParamHandler(BaseAudioContext& context,
                                     AudioParamType param_type,
                                     String param_name,
                                     double default_value,
                                     float min_value,
                                     float max_value)
    : AudioSummingJunction(context.GetDeferredTaskHandler()),
      param_type_(param_type),
      param_name_(param_name),
      intrinsic_value_(default_value),
      default_value_(default_value),
      min_value_(min_value),
      max_value_(max_value) {
  // The destination MUST exist because we need the destination handler for the
  // AudioParam.
  CHECK(context.destination());

  destination_handler_ = &context.destination()->GetAudioDestinationHandler();
  timeline_.SetSmoothedValue(default_value);
}

AudioDestinationHandler& AudioParamHandler::DestinationHandler() const {
  return *destination_handler_;
}

void AudioParamHandler::SetParamType(AudioParamType param_type) {
  param_type_ = param_type;
}

String AudioParamHandler::GetParamName() const {
  return param_name_;
}

float AudioParamHandler::Value() {
  // Update value for timeline.
  float v = IntrinsicValue();
  if (GetDeferredTaskHandler().IsAudioThread()) {
    bool has_value;
    float timeline_value = timeline_.ValueForContextTime(
        DestinationHandler(), v, has_value, MinValue(), MaxValue());

    if (has_value)
      v = timeline_value;
  }

  SetIntrinsicValue(v);
  return v;
}

void AudioParamHandler::SetIntrinsicValue(float new_value) {
  new_value = clampTo(new_value, min_value_, max_value_);
  NoBarrierStore(&intrinsic_value_, new_value);
}

void AudioParamHandler::SetValue(float value) {
  SetIntrinsicValue(value);
}

float AudioParamHandler::SmoothedValue() {
  return timeline_.SmoothedValue();
}

bool AudioParamHandler::Smooth() {
  // If values have been explicitly scheduled on the timeline, then use the
  // exact value.  Smoothing effectively is performed by the timeline.
  bool use_timeline_value = false;
  float value =
      timeline_.ValueForContextTime(DestinationHandler(), IntrinsicValue(),
                                    use_timeline_value, MinValue(), MaxValue());

  float smoothed_value = timeline_.SmoothedValue();
  if (smoothed_value == value) {
    // Smoothed value has already approached and snapped to value.
    SetIntrinsicValue(value);
    return true;
  }

  if (use_timeline_value) {
    timeline_.SetSmoothedValue(value);
  } else {
    // Dezipper - exponential approach.
    smoothed_value += (value - smoothed_value) * kDefaultSmoothingConstant;

    // If we get close enough then snap to actual value.
    // FIXME: the threshold needs to be adjustable depending on range - but
    // this is OK general purpose value.
    if (fabs(smoothed_value - value) < kSnapThreshold)
      smoothed_value = value;
    timeline_.SetSmoothedValue(smoothed_value);
  }

  SetIntrinsicValue(value);
  return false;
}

float AudioParamHandler::FinalValue() {
  float value = IntrinsicValue();
  CalculateFinalValues(&value, 1, false);
  return value;
}

void AudioParamHandler::CalculateSampleAccurateValues(
    float* values,
    unsigned number_of_values) {
  bool is_safe =
      GetDeferredTaskHandler().IsAudioThread() && values && number_of_values;
  DCHECK(is_safe);
  if (!is_safe)
    return;

  CalculateFinalValues(values, number_of_values, true);
}

void AudioParamHandler::CalculateFinalValues(float* values,
                                             unsigned number_of_values,
                                             bool sample_accurate) {
  bool is_good =
      GetDeferredTaskHandler().IsAudioThread() && values && number_of_values;
  DCHECK(is_good);
  if (!is_good)
    return;

  // The calculated result will be the "intrinsic" value summed with all
  // audio-rate connections.

  if (sample_accurate) {
    // Calculate sample-accurate (a-rate) intrinsic values.
    CalculateTimelineValues(values, number_of_values);
  } else {
    // Calculate control-rate (k-rate) intrinsic value.
    bool has_value;
    float value = IntrinsicValue();
    float timeline_value = timeline_.ValueForContextTime(
        DestinationHandler(), value, has_value, MinValue(), MaxValue());

    if (has_value)
      value = timeline_value;

    values[0] = value;
    SetIntrinsicValue(value);
  }

  // Now sum all of the audio-rate connections together (unity-gain summing
  // junction).  Note that connections would normally be mono, but we mix down
  // to mono if necessary.
  scoped_refptr<AudioBus> summing_bus =
      AudioBus::Create(1, number_of_values, false);
  summing_bus->SetChannelMemory(0, values, number_of_values);

  for (unsigned i = 0; i < NumberOfRenderingConnections(); ++i) {
    AudioNodeOutput* output = RenderingOutput(i);
    DCHECK(output);

    // Render audio from this output.
    AudioBus* connection_bus =
        output->Pull(nullptr, AudioUtilities::kRenderQuantumFrames);

    // Sum, with unity-gain.
    summing_bus->SumFrom(*connection_bus);
  }
}

void AudioParamHandler::CalculateTimelineValues(float* values,
                                                unsigned number_of_values) {
  // Calculate values for this render quantum.  Normally
  // |numberOfValues| will equal to
  // AudioUtilities::kRenderQuantumFrames (the render quantum size).
  double sample_rate = DestinationHandler().SampleRate();
  size_t start_frame = DestinationHandler().CurrentSampleFrame();
  size_t end_frame = start_frame + number_of_values;

  // Note we're running control rate at the sample-rate.
  // Pass in the current value as default value.
  SetIntrinsicValue(timeline_.ValuesForFrameRange(
      start_frame, end_frame, IntrinsicValue(), values, number_of_values,
      sample_rate, sample_rate, MinValue(), MaxValue()));
}

void AudioParamHandler::Connect(AudioNodeOutput& output) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  if (outputs_.Contains(&output))
    return;

  output.AddParam(*this);
  outputs_.insert(&output);
  ChangedOutputs();
}

void AudioParamHandler::Disconnect(AudioNodeOutput& output) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  if (outputs_.Contains(&output)) {
    outputs_.erase(&output);
    ChangedOutputs();
    output.RemoveParam(*this);
  }
}

int AudioParamHandler::ComputeQHistogramValue(float new_value) const {
  // For the Q value, assume a useful range is [0, 25] and that 0.25 dB
  // resolution is good enough.  Then, we can map the floating point Q value (in
  // dB) to an integer just by multipling by 4 and rounding.
  new_value = clampTo(new_value, 0.0, 25.0);
  return static_cast<int>(4 * new_value + 0.5);
}

// ----------------------------------------------------------------

// TODO(crbug.com/764396): Remove this when fixed.
bool AudioParam::s_value_setter_warning_done_ = false;

AudioParam::AudioParam(BaseAudioContext& context,
                       AudioParamType param_type,
                       String param_name,
                       double default_value,
                       float min_value,
                       float max_value)
    : handler_(AudioParamHandler::Create(context,
                                         param_type,
                                         param_name,
                                         default_value,
                                         min_value,
                                         max_value)),
      context_(context) {}

AudioParam* AudioParam::Create(BaseAudioContext& context,
                               AudioParamType param_type,
                               String param_name,
                               double default_value,
                               float min_value,
                               float max_value) {
  DCHECK_LE(min_value, max_value);
  return new AudioParam(context, param_type, param_name, default_value,
                        min_value, max_value);
}

void AudioParam::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  ScriptWrappable::Trace(visitor);
}

float AudioParam::value() const {
  return Handler().Value();
}

void AudioParam::WarnIfOutsideRange(const String& param_method, float value) {
  if (value < minValue() || value > maxValue()) {
    Context()->GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        Handler().GetParamName() + "." + param_method + " " +
            String::Number(value) + " outside nominal range [" +
            String::Number(minValue()) + ", " + String::Number(maxValue()) +
            "]; value will be clamped."));
  }
}

void AudioParam::setInitialValue(float value) {
  WarnIfOutsideRange("value", value);
  Handler().SetValue(value);
}

void AudioParam::setValue(float value) {
  // These nodes have dezippering which is being removed.  Print a
  // deprecation message.
  // TODO(rtoy): Remove this when dezippering has been removed.
  switch (GetParamType()) {
    case kParamTypeBiquadFilterFrequency:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperBiquadFilterNodeFrequency);
      break;
    case kParamTypeBiquadFilterQ:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperBiquadFilterNodeQ);
      break;
    case kParamTypeBiquadFilterGain:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperBiquadFilterNodeGain);
      break;
    case kParamTypeBiquadFilterDetune:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperBiquadFilterNodeDetune);
      break;
    case kParamTypeDelayDelayTime:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperDelayNodeDelayTime);
      break;
    case kParamTypeGainGain:
      Deprecation::CountDeprecation(Context()->GetExecutionContext(),
                                    WebFeature::kWebAudioDezipperGainNodeGain);
      break;
    case kParamTypeOscillatorFrequency:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperOscillatorNodeFrequency);
      break;
    case kParamTypeOscillatorDetune:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperOscillatorNodeDetune);
      break;
    case kParamTypeStereoPannerPan:
      Deprecation::CountDeprecation(
          Context()->GetExecutionContext(),
          WebFeature::kWebAudioDezipperStereoPannerNodePan);
      break;
    default:
      break;
  };

  // TODO(crbug.com/764396): Remove this when fixed.
  WarnIfSetterOverlapsEvent();

  WarnIfOutsideRange("value", value);
  Handler().SetValue(value);
}

// TODO(crbug.com/764396): Remove this when fixed.
void AudioParam::WarnIfSetterOverlapsEvent() {
  DCHECK(IsMainThread());

  // Check for overlap and print a warning only if we haven't already
  // printed a warning.
  Handler().Timeline().WarnIfSetterOverlapsEvent(
      Context(), Handler().GetParamName(), !s_value_setter_warning_done_);
}

float AudioParam::defaultValue() const {
  return Handler().DefaultValue();
}

float AudioParam::minValue() const {
  return Handler().MinValue();
}

float AudioParam::maxValue() const {
  return Handler().MaxValue();
}

void AudioParam::SetParamType(AudioParamType param_type) {
  Handler().SetParamType(param_type);
}

AudioParam* AudioParam::setValueAtTime(float value,
                                       double time,
                                       ExceptionState& exception_state) {
  WarnIfOutsideRange("setValueAtTime value", value);
  Handler().Timeline().SetValueAtTime(value, time, exception_state);
  return this;
}

AudioParam* AudioParam::linearRampToValueAtTime(
    float value,
    double time,
    ExceptionState& exception_state) {
  WarnIfOutsideRange("linearRampToValueAtTime value", value);
  Handler().Timeline().LinearRampToValueAtTime(
      value, time, Handler().IntrinsicValue(), Context()->currentTime(),
      exception_state);

  return this;
}

AudioParam* AudioParam::exponentialRampToValueAtTime(
    float value,
    double time,
    ExceptionState& exception_state) {
  WarnIfOutsideRange("exponentialRampToValue value", value);
  Handler().Timeline().ExponentialRampToValueAtTime(
      value, time, Handler().IntrinsicValue(), Context()->currentTime(),
      exception_state);

  return this;
}

AudioParam* AudioParam::setTargetAtTime(float target,
                                        double time,
                                        double time_constant,
                                        ExceptionState& exception_state) {
  WarnIfOutsideRange("setTargetAtTime value", target);
  Handler().Timeline().SetTargetAtTime(target, time, time_constant,
                                       exception_state);

  // Don't update the histogram here.  It's not clear in normal usage if the
  // parameter value will actually reach |target|.
  return this;
}

AudioParam* AudioParam::setValueCurveAtTime(const Vector<float>& curve,
                                            double time,
                                            double duration,
                                            ExceptionState& exception_state) {
  float min = minValue();
  float max = maxValue();

  // Find the first value in the curve (if any) that is outside the
  // nominal range.  It's probably not necessary to produce a warning
  // on every value outside the nominal range.
  for (unsigned k = 0; k < curve.size(); ++k) {
    float value = curve[k];

    if (value < min || value > max) {
      WarnIfOutsideRange("setValueCurveAtTime value", value);
      break;
    }
  }

  Handler().Timeline().SetValueCurveAtTime(curve, time, duration,
                                           exception_state);

  // We could update the histogram with every value in the curve, due to
  // interpolation, we'll probably be missing many values.  So we don't update
  // the histogram.  setValueCurveAtTime is probably a fairly rare method
  // anyway.
  return this;
}

AudioParam* AudioParam::cancelScheduledValues(double start_time,
                                              ExceptionState& exception_state) {
  Handler().Timeline().CancelScheduledValues(start_time, exception_state);
  return this;
}

AudioParam* AudioParam::cancelAndHoldAtTime(double start_time,
                                            ExceptionState& exception_state) {
  Handler().Timeline().CancelAndHoldAtTime(start_time, exception_state);
  return this;
}

}  // namespace blink
