/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "modules/webaudio/AudioParamTimeline.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "platform/FloatConversion.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/CPU.h"
#include "wtf/MathExtras.h"
#include <algorithm>

#if CPU(X86) || CPU(X86_64)
#include <emmintrin.h>
#endif

namespace blink {

static bool isPositiveAudioParamValue(float value, ExceptionState& exceptionState)
{
    if (value > 0)
        return true;

    // Use denorm_min() in error message to make it clear what the mininum positive value is. The
    // Javascript API uses doubles, which gets converted to floats, sometimes causing an underflow.
    // This is confusing if the user specified a small non-zero (double) value that underflowed to
    // 0.
    exceptionState.throwDOMException(
        InvalidAccessError,
        ExceptionMessages::indexOutsideRange("float target value",
            value,
            std::numeric_limits<float>::denorm_min(),
            ExceptionMessages::InclusiveBound,
            std::numeric_limits<float>::infinity(),
            ExceptionMessages::ExclusiveBound));
    return false;
}

static bool isNonNegativeAudioParamTime(double time, ExceptionState& exceptionState, String message = "Time")
{
    if (time >= 0)
        return true;

    exceptionState.throwDOMException(
        InvalidAccessError,
        message + " must be a finite non-negative number: " + String::number(time));
    return false;
}

static bool isPositiveAudioParamTime(double time, ExceptionState& exceptionState, String message)
{
    if (time > 0)
        return true;

    exceptionState.throwDOMException(
        InvalidAccessError,
        message + " must be a finite positive number: " + String::number(time));
    return false;
}

String AudioParamTimeline::eventToString(const ParamEvent& event)
{
    // The default arguments for most automation methods is the value and the time.
    String args = String::number(event.value()) + ", " + String::number(event.time(), 16);

    // Get a nice printable name for the event and update the args if necessary.
    String s;
    switch (event.type()) {
    case ParamEvent::SetValue:
        s = "setValueAtTime";
        break;
    case ParamEvent::LinearRampToValue:
        s = "linearRampToValueAtTime";
        break;
    case ParamEvent::ExponentialRampToValue:
        s = "exponentialRampToValue";
        break;
    case ParamEvent::SetTarget:
        s = "setTargetAtTime";
        // This has an extra time constant arg
        args = args + ", " + String::number(event.timeConstant(), 16);
        break;
    case ParamEvent::SetValueCurve:
        s = "setValueCurveAtTime";
        // Replace the default arg, using "..." to denote the curve argument.
        args = "..., " + String::number(event.time(), 16) + ", " + String::number(event.duration(), 16);
        break;
    case ParamEvent::LastType:
        ASSERT_NOT_REACHED();
        break;
    };

    return s + "(" + args + ")";
}

void AudioParamTimeline::setValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState))
        return;

    insertEvent(ParamEvent(ParamEvent::SetValue, value, time, 0, 0, nullptr), exceptionState);
}

void AudioParamTimeline::linearRampToValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState))
        return;

    insertEvent(ParamEvent(ParamEvent::LinearRampToValue, value, time, 0, 0, nullptr), exceptionState);
}

void AudioParamTimeline::exponentialRampToValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isPositiveAudioParamValue(value, exceptionState)
        || !isNonNegativeAudioParamTime(time, exceptionState))
        return;

    insertEvent(ParamEvent(ParamEvent::ExponentialRampToValue, value, time, 0, 0, nullptr), exceptionState);
}

void AudioParamTimeline::setTargetAtTime(float target, double time, double timeConstant, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState)
        || !isNonNegativeAudioParamTime(timeConstant, exceptionState, "Time constant"))
        return;

    insertEvent(ParamEvent(ParamEvent::SetTarget, target, time, timeConstant, 0, nullptr), exceptionState);
}

void AudioParamTimeline::setValueCurveAtTime(DOMFloat32Array* curve, double time, double duration, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState)
        || !isPositiveAudioParamTime(duration, exceptionState, "Duration"))
        return;

    insertEvent(ParamEvent(ParamEvent::SetValueCurve, 0, time, 0, duration, curve), exceptionState);
}

void AudioParamTimeline::insertEvent(const ParamEvent& event, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    // Sanity check the event. Be super careful we're not getting infected with NaN or Inf. These
    // should have been handled by the caller.
    bool isValid = event.type() < ParamEvent::LastType
        && std::isfinite(event.value())
        && std::isfinite(event.time())
        && std::isfinite(event.timeConstant())
        && std::isfinite(event.duration())
        && event.duration() >= 0;

    ASSERT(isValid);
    if (!isValid)
        return;

    MutexLocker locker(m_eventsLock);

    unsigned i = 0;
    double insertTime = event.time();

    for (i = 0; i < m_events.size(); ++i) {
        if (event.type() == ParamEvent::SetValueCurve) {
            // If this event is a SetValueCurve, make sure it doesn't overlap any existing
            // event. It's ok if the SetValueCurve starts at the same time as the end of some other
            // duration.
            double endTime = event.time() + event.duration();
            if (m_events[i].time() > event.time() && m_events[i].time() < endTime) {
                exceptionState.throwDOMException(
                    NotSupportedError,
                    eventToString(event) + " overlaps " + eventToString(m_events[i]));
                return;
            }
        } else {
            // Otherwise, make sure this event doesn't overlap any existing SetValueCurve event.
            if (m_events[i].type() == ParamEvent::SetValueCurve) {
                double endTime = m_events[i].time() + m_events[i].duration();
                if (event.time() >= m_events[i].time() && event.time() < endTime) {
                    exceptionState.throwDOMException(
                        NotSupportedError,
                        eventToString(event) + " overlaps " + eventToString(m_events[i]));
                    return;
                }
            }
        }

        // Overwrite same event type and time.
        if (m_events[i].time() == insertTime && m_events[i].type() == event.type()) {
            m_events[i] = event;
            return;
        }

        if (m_events[i].time() > insertTime)
            break;
    }

    m_events.insert(i, event);
}

bool AudioParamTimeline::hasValues() const
{
    MutexTryLocker tryLocker(m_eventsLock);

    if (tryLocker.locked())
        return m_events.size();

    // Can't get the lock so that means the main thread is trying to insert an event.  Just
    // return true then.  If the main thread releases the lock before valueForContextTime or
    // valuesForFrameRange runs, then the there will be an event on the timeline, so everything
    // is fine.  If the lock is held so that neither valueForContextTime nor valuesForFrameRange
    // can run, this is ok too, because they have tryLocks to produce a default value.  The
    // event will then get processed in the next rendering quantum.
    //
    // Don't want to return false here because that would confuse the processing of the timeline
    // if previously we returned true and now suddenly return false, only to return true on the
    // next rendering quantum.  Currently, once a timeline has been introduced it is always true
    // forever because m_events never shrinks.
    return true;
}

void AudioParamTimeline::cancelScheduledValues(double startTime, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    MutexLocker locker(m_eventsLock);

    // Remove all events starting at startTime.
    for (unsigned i = 0; i < m_events.size(); ++i) {
        if (m_events[i].time() >= startTime) {
            m_events.remove(i, m_events.size() - i);
            break;
        }
    }
}

float AudioParamTimeline::valueForContextTime(AbstractAudioContext* context, float defaultValue, bool& hasValue)
{
    ASSERT(context);

    {
        MutexTryLocker tryLocker(m_eventsLock);
        if (!tryLocker.locked() || !context || !m_events.size() || context->currentTime() < m_events[0].time()) {
            hasValue = false;
            return defaultValue;
        }
    }

    // Ask for just a single value.
    float value;
    double sampleRate = context->sampleRate();
    size_t startFrame = context->currentSampleFrame();
    double controlRate = sampleRate / AudioHandler::ProcessingSizeInFrames; // one parameter change per render quantum
    value = valuesForFrameRange(startFrame, startFrame + 1, defaultValue, &value, 1, sampleRate, controlRate);

    hasValue = true;
    return value;
}

float AudioParamTimeline::valuesForFrameRange(
    size_t startFrame,
    size_t endFrame,
    float defaultValue,
    float* values,
    unsigned numberOfValues,
    double sampleRate,
    double controlRate)
{
    // We can't contend the lock in the realtime audio thread.
    MutexTryLocker tryLocker(m_eventsLock);
    if (!tryLocker.locked()) {
        if (values) {
            for (unsigned i = 0; i < numberOfValues; ++i)
                values[i] = defaultValue;
        }
        return defaultValue;
    }

    return valuesForFrameRangeImpl(startFrame, endFrame, defaultValue, values, numberOfValues, sampleRate, controlRate);
}

float AudioParamTimeline::valuesForFrameRangeImpl(
    size_t startFrame,
    size_t endFrame,
    float defaultValue,
    float* values,
    unsigned numberOfValues,
    double sampleRate,
    double controlRate)
{
    ASSERT(values);
    if (!values)
        return defaultValue;

    // Return default value if there are no events matching the desired time range.
    if (!m_events.size() || (endFrame / sampleRate <= m_events[0].time())) {
        for (unsigned i = 0; i < numberOfValues; ++i)
            values[i] = defaultValue;
        return defaultValue;
    }

    // Maintain a running time (frame) and index for writing the values buffer.
    size_t currentFrame = startFrame;
    unsigned writeIndex = 0;

    // If first event is after startFrame then fill initial part of values buffer with defaultValue
    // until we reach the first event time.
    double firstEventTime = m_events[0].time();
    if (firstEventTime > startFrame / sampleRate) {
        // |fillToFrame| is an exclusive upper bound, so use ceil() to compute the bound from the
        // firstEventTime.
        size_t fillToFrame = std::min(endFrame, static_cast<size_t>(ceil(firstEventTime * sampleRate)));
        ASSERT(fillToFrame >= startFrame);
        fillToFrame -= startFrame;
        fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));
        for (; writeIndex < fillToFrame; ++writeIndex)
            values[writeIndex] = defaultValue;

        currentFrame += fillToFrame;
    }

    float value = defaultValue;

    // Go through each event and render the value buffer where the times overlap,
    // stopping when we've rendered all the requested values.
    // FIXME: could try to optimize by avoiding having to iterate starting from the very first event
    // and keeping track of a "current" event index.
    int n = m_events.size();
    for (int i = 0; i < n && writeIndex < numberOfValues; ++i) {
        ParamEvent& event = m_events[i];
        ParamEvent* nextEvent = i < n - 1 ? &(m_events[i + 1]) : 0;

        // Wait until we get a more recent event.
        if (nextEvent && nextEvent->time() < currentFrame / sampleRate) {
            // But if the current event is a SetValue event and the event time is between
            // currentFrame - 1 and curentFrame (in time). we don't want to skip it.  If we do skip
            // it, the SetValue event is completely skipped and not applied, which is wrong.  Other
            // events don't have this problem.  (Because currentFrame is unsigned, we do the time
            // check in this funny, but equivalent way.)
            double eventFrame = event.time() * sampleRate;

            // Condition is currentFrame - 1 < eventFrame <= currentFrame, but currentFrame is
            // unsigned and could be 0, so use currentFrame < eventFrame + 1 instead.
            if (!((event.type() == ParamEvent::SetValue
                && (eventFrame <= currentFrame)
                && (currentFrame < eventFrame + 1))))
                continue;
        }

        float value1 = event.value();
        double time1 = event.time();
        float value2 = nextEvent ? nextEvent->value() : value1;
        double time2 = nextEvent ? nextEvent->time() : endFrame / sampleRate + 1;

        double deltaTime = time2 - time1;
        float k = deltaTime > 0 ? 1 / deltaTime : 0;

        // |fillToEndFrame| is the exclusive upper bound of the last frame to be computed for this
        // event.  It's either the last desired frame (|endFrame|) or derived from the end time of
        // the next event (time2). We compute ceil(time2*sampleRate) because fillToEndFrame is the
        // exclusive upper bound.  Consider the case where |startFrame| = 128 and time2 = 128.1
        // (assuming sampleRate = 1).  Since time2 is greater than 128, we want to output a value
        // for frame 128.  This requires that fillToEndFrame be at least 129.  This is achieved by
        // ceil(time2).
        size_t fillToEndFrame = std::min(endFrame, static_cast<size_t>(ceil(time2 * sampleRate)));
        ASSERT(fillToEndFrame >= startFrame);
        size_t fillToFrame = fillToEndFrame - startFrame;
        fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));

        ParamEvent::Type nextEventType = nextEvent ? static_cast<ParamEvent::Type>(nextEvent->type()) : ParamEvent::LastType /* unknown */;

        // First handle linear and exponential ramps which require looking ahead to the next event.
        if (nextEventType == ParamEvent::LinearRampToValue) {
            const float valueDelta = value2 - value1;
#if CPU(X86) || CPU(X86_64)
            // Minimize in-loop operations. Calculate starting value and increment. Next step: value += inc.
            //  value = value1 + (currentFrame/sampleRate - time1) * k * (value2 - value1);
            //  inc = 4 / sampleRate * k * (value2 - value1);
            // Resolve recursion by expanding constants to achieve a 4-step loop unrolling.
            //  value = value1 + ((currentFrame/sampleRate - time1) + i * sampleFrameTimeIncr) * k * (value2 -value1), i in 0..3
            __m128 vValue = _mm_mul_ps(_mm_set_ps1(1 / sampleRate), _mm_set_ps(3, 2, 1, 0));
            vValue = _mm_add_ps(vValue, _mm_set_ps1(currentFrame / sampleRate - time1));
            vValue = _mm_mul_ps(vValue, _mm_set_ps1(k * valueDelta));
            vValue = _mm_add_ps(vValue, _mm_set_ps1(value1));
            __m128 vInc = _mm_set_ps1(4 / sampleRate * k * valueDelta);

            // Truncate loop steps to multiple of 4.
            unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
            // Compute final time.
            currentFrame += fillToFrameTrunc - writeIndex;

            // Process 4 loop steps.
            for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
                _mm_storeu_ps(values + writeIndex, vValue);
                vValue = _mm_add_ps(vValue, vInc);
            }
#endif
            // Serially process remaining values.
            for (; writeIndex < fillToFrame; ++writeIndex) {
                float x = (currentFrame / sampleRate - time1) * k;
                // value = (1 - x) * value1 + x * value2;
                value = value1 + x * valueDelta;
                values[writeIndex] = value;
                ++currentFrame;
            }
        } else if (nextEventType == ParamEvent::ExponentialRampToValue) {
            if (value1 <= 0 || value2 <= 0) {
                // Handle negative values error case by propagating previous value.
                for (; writeIndex < fillToFrame; ++writeIndex)
                    values[writeIndex] = value;
            } else {
                float numSampleFrames = deltaTime * sampleRate;
                // The value goes exponentially from value1 to value2 in a duration of deltaTime
                // seconds according to
                //
                //  v(t) = v1*(v2/v1)^((t-t1)/(t2-t1))
                //
                // Let c be currentFrame and F be the sampleRate.  Then we want to sample v(t)
                // at times t = (c + k)/F for k = 0, 1, ...:
                //
                //   v((c+k)/F) = v1*(v2/v1)^(((c/F+k/F)-t1)/(t2-t1))
                //              = v1*(v2/v1)^((c/F-t1)/(t2-t1))
                //                  *(v2/v1)^((k/F)/(t2-t1))
                //              = v1*(v2/v1)^((c/F-t1)/(t2-t1))
                //                  *[(v2/v1)^(1/(F*(t2-t1)))]^k
                //
                // Thus, this can be written as
                //
                //   v((c+k)/F) = V*m^k
                //
                // where
                //   V = v1*(v2/v1)^((c/F-t1)/(t2-t1))
                //   m = (v2/v1)^(1/(F*(t2-t1)))

                // Compute the per-sample multiplier.
                float multiplier = powf(value2 / value1, 1 / numSampleFrames);
                // Set the starting value of the exponential ramp.
                value = value1 * powf(value2 / value1,
                    (currentFrame / sampleRate - time1) / deltaTime);

                for (; writeIndex < fillToFrame; ++writeIndex) {
                    values[writeIndex] = value;
                    value *= multiplier;
                    ++currentFrame;
                }
            }
        } else {
            // Handle event types not requiring looking ahead to the next event.
            switch (event.type()) {
            case ParamEvent::SetValue:
            case ParamEvent::LinearRampToValue:
            case ParamEvent::ExponentialRampToValue:
                {
                    currentFrame = fillToEndFrame;

                    // Simply stay at a constant value.
                    value = event.value();
                    for (; writeIndex < fillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    break;
                }

            case ParamEvent::SetTarget:
                {
                    // Exponential approach to target value with given time constant.
                    //
                    //   v(t) = v2 + (v1 - v2)*exp(-(t-t1/tau))
                    //

                    float target = event.value();
                    float timeConstant = event.timeConstant();
                    float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(timeConstant, controlRate));

                    // Set the starting value correctly.  This is only needed when the current time
                    // is "equal" to the start time of this event.  This is to get the sampling
                    // correct if the start time of this automation isn't on a frame boundary.
                    // Otherwise, we can just continue from where we left off from the previous
                    // rendering quantum.

                    {
                        double rampStartFrame = time1 * sampleRate;
                        // Condition is c - 1 < r <= c where c = currentFrame and r =
                        // rampStartFrame.  Compute it this way because currentFrame is unsigned and
                        // could be 0.
                        if (rampStartFrame <= currentFrame && currentFrame < rampStartFrame + 1)
                            value = target + (value - target) * exp(-(currentFrame / sampleRate - time1) / timeConstant);
                    }
#if CPU(X86) || CPU(X86_64)
                    // Resolve recursion by expanding constants to achieve a 4-step loop unrolling.
                    // v1 = v0 + (t - v0) * c
                    // v2 = v1 + (t - v1) * c
                    // v2 = v0 + (t - v0) * c + (t - (v0 + (t - v0) * c)) * c
                    // v2 = v0 + (t - v0) * c + (t - v0) * c - (t - v0) * c * c
                    // v2 = v0 + (t - v0) * c * (2 - c)
                    // Thus c0 = c, c1 = c*(2-c). The same logic applies to c2 and c3.
                    const float c0 = discreteTimeConstant;
                    const float c1 = c0 * (2 - c0);
                    const float c2 = c0 * ((c0 - 3) * c0 + 3);
                    const float c3 = c0 * (c0 * ((4 - c0) * c0 - 6) + 4);

                    float delta;
                    __m128 vC = _mm_set_ps(c2, c1, c0, 0);
                    __m128 vDelta, vValue, vResult;

                    // Process 4 loop steps.
                    unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
                    for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
                        delta = target - value;
                        vDelta = _mm_set_ps1(delta);
                        vValue = _mm_set_ps1(value);

                        vResult = _mm_add_ps(vValue, _mm_mul_ps(vDelta, vC));
                        _mm_storeu_ps(values + writeIndex, vResult);

                        // Update value for next iteration.
                        value += delta * c3;
                    }
#endif
                    // Serially process remaining values
                    for (; writeIndex < fillToFrame; ++writeIndex) {
                        values[writeIndex] = value;
                        value += (target - value) * discreteTimeConstant;
                    }

                    currentFrame = fillToEndFrame;

                    break;
                }

            case ParamEvent::SetValueCurve:
                {
                    DOMFloat32Array* curve = event.curve();
                    float* curveData = curve ? curve->data() : 0;
                    unsigned numberOfCurvePoints = curve ? curve->length() : 0;

                    // Curve events have duration, so don't just use next event time.
                    double duration = event.duration();
                    double durationFrames = duration * sampleRate;
                    // How much to step the curve index for each frame.  We want the curve index to
                    // be exactly equal to the last index (numberOfCurvePoints - 1) after
                    // durationFrames - 1 frames.  In this way, the last output value will equal the
                    // last value in the curve array.
                    double curvePointsPerFrame;

                    // If the duration is less than a frame, we want to just output the last curve
                    // value.  Do this by setting curvePointsPerFrame to be more than number of
                    // points in the curve.  Then the curveVirtualIndex will always exceed the last
                    // curve index, so that the last curve value will be used.
                    if (durationFrames > 1)
                        curvePointsPerFrame = (numberOfCurvePoints - 1) / (durationFrames - 1);
                    else
                        curvePointsPerFrame = numberOfCurvePoints + 1;

                    if (!curve || !curveData || !numberOfCurvePoints || duration <= 0 || sampleRate <= 0) {
                        // Error condition - simply propagate previous value.
                        currentFrame = fillToEndFrame;
                        for (; writeIndex < fillToFrame; ++writeIndex)
                            values[writeIndex] = value;
                        break;
                    }

                    // Save old values and recalculate information based on the curve's duration
                    // instead of the next event time.
                    size_t nextEventFillToFrame = fillToFrame;

                    // Use ceil here for the same reason as using ceil above: fillToEndFrame is an
                    // exclusive upper bound of the last frame to be computed.
                    fillToEndFrame = std::min(endFrame, static_cast<size_t>(ceil(sampleRate*(time1 + duration))));
                    // |fillToFrame| can be less than |startFrame| when the end of the
                    // setValueCurve automation has been reached, but the next automation has not
                    // yet started. In this case, |fillToFrame| is clipped to |time1|+|duration|
                    // above, but |startFrame| will keep increasing (because the current time is
                    // increasing).
                    fillToFrame = (fillToEndFrame < startFrame) ? 0 : fillToEndFrame - startFrame;
                    fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));

                    // Index into the curve data using a floating-point value.
                    // We're scaling the number of curve points by the duration (see curvePointsPerFrame).
                    double curveVirtualIndex = 0;
                    if (time1 < currentFrame / sampleRate) {
                        // Index somewhere in the middle of the curve data.
                        // Don't use timeToSampleFrame() since we want the exact floating-point frame.
                        double frameOffset = currentFrame - time1 * sampleRate;
                        curveVirtualIndex = curvePointsPerFrame * frameOffset;
                    }

                    // Set the default value in case fillToFrame is 0.
                    value = curveData[numberOfCurvePoints - 1];

                    // Render the stretched curve data using linear interpolation.  Oversampled
                    // curve data can be provided if sharp discontinuities are desired.
                    unsigned k = 0;
#if CPU(X86) || CPU(X86_64)
                    const __m128 vCurveVirtualIndex = _mm_set_ps1(curveVirtualIndex);
                    const __m128 vCurvePointsPerFrame = _mm_set_ps1(curvePointsPerFrame);
                    const __m128 vNumberOfCurvePointsM1 = _mm_set_ps1(numberOfCurvePoints - 1);
                    const __m128 vN1 = _mm_set_ps1(1.0f);
                    const __m128 vN4 = _mm_set_ps1(4.0f);

                    __m128 vK = _mm_set_ps(3, 2, 1, 0);
                    int aCurveIndex0[4];
                    int aCurveIndex1[4];

                    // Truncate loop steps to multiple of 4
                    unsigned truncatedSteps = ((fillToFrame - writeIndex) / 4) * 4;
                    unsigned fillToFrameTrunc = writeIndex + truncatedSteps;
                    for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
                        // Compute current index this way to minimize round-off that would have
                        // occurred by incrementing the index by curvePointsPerFrame.
                        __m128 vCurrentVirtualIndex = _mm_add_ps(vCurveVirtualIndex, _mm_mul_ps(vK, vCurvePointsPerFrame));
                        vK = _mm_add_ps(vK, vN4);

                        // Clamp index to the last element of the array.
                        __m128i vCurveIndex0 = _mm_cvttps_epi32(_mm_min_ps(vCurrentVirtualIndex, vNumberOfCurvePointsM1));
                        __m128i vCurveIndex1 = _mm_cvttps_epi32(_mm_min_ps(_mm_add_ps(vCurrentVirtualIndex, vN1), vNumberOfCurvePointsM1));

                        // Linearly interpolate between the two nearest curve points.  |delta| is
                        // clamped to 1 because currentVirtualIndex can exceed curveIndex0 by more
                        // than one.  This can happen when we reached the end of the curve but still
                        // need values to fill out the current rendering quantum.
                        _mm_storeu_si128((__m128i*)aCurveIndex0, vCurveIndex0);
                        _mm_storeu_si128((__m128i*)aCurveIndex1, vCurveIndex1);
                        __m128 vC0 = _mm_set_ps(curveData[aCurveIndex0[3]], curveData[aCurveIndex0[2]], curveData[aCurveIndex0[1]], curveData[aCurveIndex0[0]]);
                        __m128 vC1 = _mm_set_ps(curveData[aCurveIndex1[3]], curveData[aCurveIndex1[2]], curveData[aCurveIndex1[1]], curveData[aCurveIndex1[0]]);
                        __m128 vDelta = _mm_min_ps(_mm_sub_ps(vCurrentVirtualIndex, _mm_cvtepi32_ps(vCurveIndex0)), vN1);

                        __m128 vValue = _mm_add_ps(vC0, _mm_mul_ps(_mm_sub_ps(vC1, vC0), vDelta));

                        _mm_storeu_ps(values + writeIndex, vValue);
                    }
                    // Pass along k to the serial loop.
                    k = truncatedSteps;
                    // If the above loop was run, pass along the last computed value.
                    if (truncatedSteps > 0) {
                        value = values[writeIndex - 1];
                    }
#endif
                    for (; writeIndex < fillToFrame; ++writeIndex, ++k) {
                        // Compute current index this way to minimize round-off that would have
                        // occurred by incrementing the index by curvePointsPerFrame.
                        double currentVirtualIndex = curveVirtualIndex + k * curvePointsPerFrame;
                        unsigned curveIndex0;

                        // Clamp index to the last element of the array.
                        if (currentVirtualIndex < numberOfCurvePoints) {
                            curveIndex0 = static_cast<unsigned>(currentVirtualIndex);
                        } else {
                            curveIndex0 = numberOfCurvePoints - 1;
                        }

                        unsigned curveIndex1 = std::min(curveIndex0 + 1, numberOfCurvePoints - 1);

                        // Linearly interpolate between the two nearest curve points.  |delta| is
                        // clamped to 1 because currentVirtualIndex can exceed curveIndex0 by more
                        // than one.  This can happen when we reached the end of the curve but still
                        // need values to fill out the current rendering quantum.
                        ASSERT(curveIndex0 < numberOfCurvePoints);
                        ASSERT(curveIndex1 < numberOfCurvePoints);
                        float c0 = curveData[curveIndex0];
                        float c1 = curveData[curveIndex1];
                        double delta = std::min(currentVirtualIndex - curveIndex0, 1.0);

                        value = c0 + (c1 - c0) * delta;

                        values[writeIndex] = value;
                    }

                    // If there's any time left after the duration of this event and the start
                    // of the next, then just propagate the last value of the curveData.
                    value = curveData[numberOfCurvePoints - 1];
                    for (; writeIndex < nextEventFillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    // Re-adjust current time
                    currentFrame = nextEventFillToFrame;

                    break;
                }
            case ParamEvent::LastType:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    // If there's any time left after processing the last event then just propagate the last value
    // to the end of the values buffer.
    for (; writeIndex < numberOfValues; ++writeIndex)
        values[writeIndex] = value;

    return value;
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
