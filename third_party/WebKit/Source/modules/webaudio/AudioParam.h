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

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioParamTimeline.h"
#include "modules/webaudio/AudioSummingJunction.h"
#include <sys/types.h>
#include "wtf/Float32Array.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AudioNodeOutput;

#if ENABLE(OILPAN)
#define AUDIO_PARAM_BASE_CLASSES public AudioSummingJunction, public ScriptWrappable
#else
#define AUDIO_PARAM_BASE_CLASSES public RefCounted<AudioParam>, public ScriptWrappable, public AudioSummingJunction
#endif
class AudioParam FINAL : AUDIO_PARAM_BASE_CLASSES {
public:
    static const double DefaultSmoothingConstant;
    static const double SnapThreshold;

    static PassRefPtrWillBeRawPtr<AudioParam> create(AudioContext* context, double defaultValue)
    {
        return adoptRefWillBeNoop(new AudioParam(context, defaultValue));
    }

    // AudioSummingJunction
#if !ENABLE(OILPAN)
    virtual bool canUpdateState() OVERRIDE { return true; }
#endif
    virtual void didUpdate() OVERRIDE { }

    // Intrinsic value.
    float value();
    void setValue(float);

    // Final value for k-rate parameters, otherwise use calculateSampleAccurateValues() for a-rate.
    // Must be called in the audio thread.
    float finalValue();

    float defaultValue() const { return static_cast<float>(m_defaultValue); }

    // Value smoothing:

    // When a new value is set with setValue(), in our internal use of the parameter we don't immediately jump to it.
    // Instead we smoothly approach this value to avoid glitching.
    float smoothedValue();

    // Smoothly exponentially approaches to (de-zippers) the desired value.
    // Returns true if smoothed value has already snapped exactly to value.
    bool smooth();

    void resetSmoothedValue() { m_smoothedValue = m_value; }

    // Parameter automation.
    void setValueAtTime(float value, double time) { m_timeline.setValueAtTime(value, time); }
    void linearRampToValueAtTime(float value, double time) { m_timeline.linearRampToValueAtTime(value, time); }
    void exponentialRampToValueAtTime(float value, double time, ExceptionState& es) { m_timeline.exponentialRampToValueAtTime(value, time, es); }
    void setTargetAtTime(float target, double time, double timeConstant) { m_timeline.setTargetAtTime(target, time, timeConstant); }
    void setValueCurveAtTime(Float32Array* curve, double time, double duration) { m_timeline.setValueCurveAtTime(curve, time, duration); }
    void cancelScheduledValues(double startTime) { m_timeline.cancelScheduledValues(startTime); }

    bool hasSampleAccurateValues() { return m_timeline.hasValues() || numberOfRenderingConnections(); }

    // Calculates numberOfValues parameter values starting at the context's current time.
    // Must be called in the context's render thread.
    void calculateSampleAccurateValues(float* values, unsigned numberOfValues);

    // Connect an audio-rate signal to control this parameter.
    void connect(AudioNodeOutput&);
    void disconnect(AudioNodeOutput&);

private:
    AudioParam(AudioContext* context, double defaultValue)
        : AudioSummingJunction(context)
        , m_value(defaultValue)
        , m_defaultValue(defaultValue)
        , m_smoothedValue(defaultValue)
    {
        ScriptWrappable::init(this);
    }

    // sampleAccurate corresponds to a-rate (audio rate) vs. k-rate in the Web Audio specification.
    void calculateFinalValues(float* values, unsigned numberOfValues, bool sampleAccurate);
    void calculateTimelineValues(float* values, unsigned numberOfValues);

    double m_value;
    double m_defaultValue;

    // Smoothing (de-zippering)
    double m_smoothedValue;

    AudioParamTimeline m_timeline;
};

} // namespace blink

#endif // AudioParam_h
