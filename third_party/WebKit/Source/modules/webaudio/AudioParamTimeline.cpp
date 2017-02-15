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

#include "modules/webaudio/AudioParamTimeline.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/CPU.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include <algorithm>

#if CPU(X86) || CPU(X86_64)
#include <emmintrin.h>
#endif

namespace blink {

// For a SetTarget event, if the relative difference between the current value
// and the target value is less than this, consider them the same and just
// output the target value.  This value MUST be larger than the single precision
// epsilon of 5.960465e-8.  Due to round-off, this value is not achievable in
// general.  This value can vary across the platforms (CPU) and thus it is
// determined experimentally.
const float kSetTargetThreshold = 1.5e-6;

// For a SetTarget event, if the target value is 0, and the current value is
// less than this threshold, consider the curve to have converged to 0.  We need
// a separate case from kSetTargetThreshold because that uses relative error,
// which is never met if the target value is 0, a common case.  This value MUST
// be larger than least positive normalized single precision
// value (1.1754944e-38) because we normally operate with flush-to-zero enabled.
const float kSetTargetZeroThreshold = 1e-20;

static bool isNonNegativeAudioParamTime(double time,
                                        ExceptionState& exceptionState,
                                        String message = "Time") {
  if (time >= 0)
    return true;

  exceptionState.throwDOMException(
      InvalidAccessError, message + " must be a finite non-negative number: " +
                              String::number(time));
  return false;
}

static bool isPositiveAudioParamTime(double time,
                                     ExceptionState& exceptionState,
                                     String message) {
  if (time > 0)
    return true;

  exceptionState.throwDOMException(
      InvalidAccessError,
      message + " must be a finite positive number: " + String::number(time));
  return false;
}

String AudioParamTimeline::eventToString(const ParamEvent& event) {
  // The default arguments for most automation methods is the value and the
  // time.
  String args =
      String::number(event.value()) + ", " + String::number(event.time(), 16);

  // Get a nice printable name for the event and update the args if necessary.
  String s;
  switch (event.getType()) {
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
      args = "..., " + String::number(event.time(), 16) + ", " +
             String::number(event.duration(), 16);
      break;
    case ParamEvent::CancelValues:
    // Fall through; we should never have to print out the internal
    // CancelValues event.
    case ParamEvent::LastType:
      ASSERT_NOT_REACHED();
      break;
  };

  return s + "(" + args + ")";
}

// Computes the value of a linear ramp event at time t with the given event
// parameters.
float AudioParamTimeline::linearRampAtTime(double t,
                                           float value1,
                                           double time1,
                                           float value2,
                                           double time2) {
  return value1 + (value2 - value1) * (t - time1) / (time2 - time1);
}

// Computes the value of an exponential ramp event at time t with the given
// event parameters.
float AudioParamTimeline::exponentialRampAtTime(double t,
                                                float value1,
                                                double time1,
                                                float value2,
                                                double time2) {
  return value1 * pow(value2 / value1, (t - time1) / (time2 - time1));
}

// Compute the value of a set target event at time t with the given event
// parameters.
float AudioParamTimeline::targetValueAtTime(double t,
                                            float value1,
                                            double time1,
                                            float value2,
                                            float timeConstant) {
  return value2 + (value1 - value2) * exp(-(t - time1) / timeConstant);
}

// Compute the value of a set curve event at time t with the given event
// parameters.
float AudioParamTimeline::valueCurveAtTime(double t,
                                           double time1,
                                           double duration,
                                           const float* curveData,
                                           unsigned curveLength) {
  double curveIndex = (curveLength - 1) / duration * (t - time1);
  unsigned k = std::min(static_cast<unsigned>(curveIndex), curveLength - 1);
  unsigned k1 = std::min(k + 1, curveLength - 1);
  float c0 = curveData[k];
  float c1 = curveData[k1];
  float delta = std::min(curveIndex - k, 1.0);

  return c0 + (c1 - c0) * delta;
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createSetValueEvent(float value, double time) {
  return WTF::wrapUnique(new ParamEvent(ParamEvent::SetValue, value, time));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createLinearRampEvent(float value,
                                                      double time,
                                                      float initialValue,
                                                      double callTime) {
  return WTF::wrapUnique(new ParamEvent(ParamEvent::LinearRampToValue, value,
                                        time, initialValue, callTime));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createExponentialRampEvent(float value,
                                                           double time,
                                                           float initialValue,
                                                           double callTime) {
  return WTF::wrapUnique(new ParamEvent(ParamEvent::ExponentialRampToValue,
                                        value, time, initialValue, callTime));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createSetTargetEvent(float value,
                                                     double time,
                                                     double timeConstant) {
  // The time line code does not expect a timeConstant of 0. (IT
  // returns NaN or Infinity due to division by zero.  The caller
  // should have converted this to a SetValueEvent.
  DCHECK_NE(timeConstant, 0);
  return WTF::wrapUnique(
      new ParamEvent(ParamEvent::SetTarget, value, time, timeConstant));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createSetValueCurveEvent(
    const DOMFloat32Array* curve,
    double time,
    double duration) {
  double curvePoints = (curve->length() - 1) / duration;
  float endValue = curve->data()[curve->length() - 1];

  return WTF::wrapUnique(new ParamEvent(
      ParamEvent::SetValueCurve, time, duration, curve, curvePoints, endValue));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createCancelValuesEvent(
    double time,
    std::unique_ptr<ParamEvent> savedEvent) {
  if (savedEvent) {
    // The savedEvent can only have certain event types.  Verify that.
    ParamEvent::Type savedType = savedEvent->getType();

    DCHECK_NE(savedType, ParamEvent::LastType);
    DCHECK(savedType == ParamEvent::LinearRampToValue ||
           savedType == ParamEvent::ExponentialRampToValue ||
           savedType == ParamEvent::SetValueCurve);
  }

  return WTF::wrapUnique(
      new ParamEvent(ParamEvent::CancelValues, time, std::move(savedEvent)));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::createGeneralEvent(
    Type type,
    float value,
    double time,
    float initialValue,
    double callTime,
    double timeConstant,
    double duration,
    Vector<float>& curve,
    double curvePointsPerSecond,
    float curveEndValue,
    std::unique_ptr<ParamEvent> savedEvent) {
  return WTF::wrapUnique(new ParamEvent(
      type, value, time, initialValue, callTime, timeConstant, duration, curve,
      curvePointsPerSecond, curveEndValue, std::move(savedEvent)));
}

AudioParamTimeline::ParamEvent* AudioParamTimeline::ParamEvent::savedEvent()
    const {
  DCHECK_EQ(getType(), ParamEvent::CancelValues);
  return m_savedEvent.get();
}

bool AudioParamTimeline::ParamEvent::hasDefaultCancelledValue() const {
  DCHECK_EQ(getType(), ParamEvent::CancelValues);
  return m_hasDefaultCancelledValue;
}

void AudioParamTimeline::ParamEvent::setCancelledValue(float value) {
  DCHECK_EQ(getType(), ParamEvent::CancelValues);
  m_value = value;
  m_hasDefaultCancelledValue = true;
}

// General event
AudioParamTimeline::ParamEvent::ParamEvent(
    ParamEvent::Type type,
    float value,
    double time,
    float initialValue,
    double callTime,
    double timeConstant,
    double duration,
    Vector<float>& curve,
    double curvePointsPerSecond,
    float curveEndValue,
    std::unique_ptr<ParamEvent> savedEvent)
    : m_type(type),
      m_value(value),
      m_time(time),
      m_initialValue(initialValue),
      m_callTime(callTime),
      m_timeConstant(timeConstant),
      m_duration(duration),
      m_curvePointsPerSecond(curvePointsPerSecond),
      m_curveEndValue(curveEndValue),
      m_savedEvent(std::move(savedEvent)),
      m_needsTimeClampCheck(true),
      m_hasDefaultCancelledValue(false) {
  m_curve = curve;
}

// Create simplest event needing just a value and time, like setValueAtTime
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           float value,
                                           double time)
    : m_type(type),
      m_value(value),
      m_time(time),
      m_initialValue(0),
      m_callTime(0),
      m_timeConstant(0),
      m_duration(0),
      m_curvePointsPerSecond(0),
      m_curveEndValue(0),
      m_savedEvent(nullptr),
      m_needsTimeClampCheck(true),
      m_hasDefaultCancelledValue(false) {
  DCHECK_EQ(type, ParamEvent::SetValue);
}

// Create a linear or exponential ramp that requires an initial value and
// time in case
// there is no actual event that preceeds this event.
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           float value,
                                           double time,
                                           float initialValue,
                                           double callTime)
    : m_type(type),
      m_value(value),
      m_time(time),
      m_initialValue(initialValue),
      m_callTime(callTime),
      m_timeConstant(0),
      m_duration(0),
      m_curvePointsPerSecond(0),
      m_curveEndValue(0),
      m_savedEvent(nullptr),
      m_needsTimeClampCheck(true),
      m_hasDefaultCancelledValue(false) {
  DCHECK(type == ParamEvent::LinearRampToValue ||
         type == ParamEvent::ExponentialRampToValue);
}

// Create an event needing a time constant (setTargetAtTime)
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           float value,
                                           double time,
                                           double timeConstant)
    : m_type(type),
      m_value(value),
      m_time(time),
      m_initialValue(0),
      m_callTime(0),
      m_timeConstant(timeConstant),
      m_duration(0),
      m_curvePointsPerSecond(0),
      m_curveEndValue(0),
      m_savedEvent(nullptr),
      m_needsTimeClampCheck(true),
      m_hasDefaultCancelledValue(false) {
  DCHECK_EQ(type, ParamEvent::SetTarget);
}

// Create a setValueCurve event
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           double time,
                                           double duration,
                                           const DOMFloat32Array* curve,
                                           double curvePointsPerSecond,
                                           float curveEndValue)
    : m_type(type),
      m_value(0),
      m_time(time),
      m_initialValue(0),
      m_callTime(0),
      m_timeConstant(0),
      m_duration(duration),
      m_curvePointsPerSecond(curvePointsPerSecond),
      m_curveEndValue(curveEndValue),
      m_savedEvent(nullptr),
      m_needsTimeClampCheck(true),
      m_hasDefaultCancelledValue(false) {
  DCHECK_EQ(type, ParamEvent::SetValueCurve);
  if (curve) {
    unsigned curveLength = curve->length();
    m_curve.resize(curve->length());
    memcpy(m_curve.data(), curve->data(), curveLength * sizeof(float));
  }
}

// Create CancelValues event
AudioParamTimeline::ParamEvent::ParamEvent(
    ParamEvent::Type type,
    double time,
    std::unique_ptr<ParamEvent> savedEvent)
    : m_type(type),
      m_value(0),
      m_time(time),
      m_initialValue(0),
      m_callTime(0),
      m_timeConstant(0),
      m_duration(0),
      m_curvePointsPerSecond(0),
      m_curveEndValue(0),
      m_savedEvent(std::move(savedEvent)),
      m_needsTimeClampCheck(true),
      m_hasDefaultCancelledValue(false) {
  DCHECK_EQ(type, ParamEvent::CancelValues);
}

void AudioParamTimeline::setValueAtTime(float value,
                                        double time,
                                        ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (!isNonNegativeAudioParamTime(time, exceptionState))
    return;

  MutexLocker locker(m_eventsLock);
  insertEvent(ParamEvent::createSetValueEvent(value, time), exceptionState);
}

void AudioParamTimeline::linearRampToValueAtTime(
    float value,
    double time,
    float initialValue,
    double callTime,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (!isNonNegativeAudioParamTime(time, exceptionState))
    return;

  MutexLocker locker(m_eventsLock);
  insertEvent(
      ParamEvent::createLinearRampEvent(value, time, initialValue, callTime),
      exceptionState);
}

void AudioParamTimeline::exponentialRampToValueAtTime(
    float value,
    double time,
    float initialValue,
    double callTime,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (!isNonNegativeAudioParamTime(time, exceptionState))
    return;

  if (!value) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The float target value provided (" + String::number(value) +
            ") should not be in the range (" +
            String::number(-std::numeric_limits<float>::denorm_min()) + ", " +
            String::number(std::numeric_limits<float>::denorm_min()) + ").");
    return;
  }

  MutexLocker locker(m_eventsLock);
  insertEvent(ParamEvent::createExponentialRampEvent(value, time, initialValue,
                                                     callTime),
              exceptionState);
}

void AudioParamTimeline::setTargetAtTime(float target,
                                         double time,
                                         double timeConstant,
                                         ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (!isNonNegativeAudioParamTime(time, exceptionState) ||
      !isNonNegativeAudioParamTime(timeConstant, exceptionState,
                                   "Time constant"))
    return;

  MutexLocker locker(m_eventsLock);

  // If timeConstant = 0, we instantly jump to the target value, so
  // insert a SetValueEvent instead of SetTargetEvent.
  if (timeConstant == 0) {
    insertEvent(ParamEvent::createSetValueEvent(target, time), exceptionState);
  } else {
    insertEvent(ParamEvent::createSetTargetEvent(target, time, timeConstant),
                exceptionState);
  }
}

void AudioParamTimeline::setValueCurveAtTime(DOMFloat32Array* curve,
                                             double time,
                                             double duration,
                                             ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  DCHECK(curve);

  if (!isNonNegativeAudioParamTime(time, exceptionState) ||
      !isPositiveAudioParamTime(duration, exceptionState, "Duration"))
    return;

  if (curve->length() < 2) {
    exceptionState.throwDOMException(
        InvalidStateError, ExceptionMessages::indexExceedsMinimumBound(
                               "curve length", curve->length(), 2U));
    return;
  }

  MutexLocker locker(m_eventsLock);
  insertEvent(ParamEvent::createSetValueCurveEvent(curve, time, duration),
              exceptionState);

  // Insert a setValueAtTime event too to establish an event so that all
  // following events will process from the end of the curve instead of the
  // beginning.
  insertEvent(ParamEvent::createSetValueEvent(
                  curve->data()[curve->length() - 1], time + duration),
              exceptionState);
}

void AudioParamTimeline::insertEvent(std::unique_ptr<ParamEvent> event,
                                     ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // Sanity check the event. Be super careful we're not getting infected with
  // NaN or Inf. These should have been handled by the caller.
  bool isValid = event->getType() < ParamEvent::LastType &&
                 std::isfinite(event->value()) &&
                 std::isfinite(event->time()) &&
                 std::isfinite(event->timeConstant()) &&
                 std::isfinite(event->duration()) && event->duration() >= 0;

  DCHECK(isValid);
  if (!isValid)
    return;

  unsigned i = 0;
  double insertTime = event->time();

  if (!m_events.size() &&
      (event->getType() == ParamEvent::LinearRampToValue ||
       event->getType() == ParamEvent::ExponentialRampToValue)) {
    // There are no events preceding these ramps.  Insert a new setValueAtTime
    // event to set the starting point for these events.
    m_events.insert(0, AudioParamTimeline::ParamEvent::createSetValueEvent(
                           event->initialValue(), event->callTime()));
  }

  for (i = 0; i < m_events.size(); ++i) {
    if (event->getType() == ParamEvent::SetValueCurve) {
      // If this event is a SetValueCurve, make sure it doesn't overlap any
      // existing event. It's ok if the SetValueCurve starts at the same time as
      // the end of some other duration.
      double endTime = event->time() + event->duration();
      if (m_events[i]->time() > event->time() &&
          m_events[i]->time() < endTime) {
        exceptionState.throwDOMException(
            NotSupportedError,
            eventToString(*event) + " overlaps " + eventToString(*m_events[i]));
        return;
      }
    } else {
      // Otherwise, make sure this event doesn't overlap any existing
      // SetValueCurve event.
      if (m_events[i]->getType() == ParamEvent::SetValueCurve) {
        double endTime = m_events[i]->time() + m_events[i]->duration();
        if (event->time() >= m_events[i]->time() && event->time() < endTime) {
          exceptionState.throwDOMException(
              NotSupportedError, eventToString(*event) + " overlaps " +
                                     eventToString(*m_events[i]));
          return;
        }
      }
    }

    // Overwrite same event type and time.
    if (m_events[i]->time() == insertTime &&
        m_events[i]->getType() == event->getType()) {
      m_events[i] = std::move(event);
      return;
    }

    if (m_events[i]->time() > insertTime)
      break;
  }

  m_events.insert(i, std::move(event));
}

bool AudioParamTimeline::hasValues() const {
  MutexTryLocker tryLocker(m_eventsLock);

  if (tryLocker.locked())
    return m_events.size();

  // Can't get the lock so that means the main thread is trying to insert an
  // event.  Just return true then.  If the main thread releases the lock before
  // valueForContextTime or valuesForFrameRange runs, then the there will be an
  // event on the timeline, so everything is fine.  If the lock is held so that
  // neither valueForContextTime nor valuesForFrameRange can run, this is ok
  // too, because they have tryLocks to produce a default value.  The event will
  // then get processed in the next rendering quantum.
  //
  // Don't want to return false here because that would confuse the processing
  // of the timeline if previously we returned true and now suddenly return
  // false, only to return true on the next rendering quantum.  Currently, once
  // a timeline has been introduced it is always true forever because m_events
  // never shrinks.
  return true;
}

void AudioParamTimeline::cancelScheduledValues(double startTime,
                                               ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  MutexLocker locker(m_eventsLock);

  // Remove all events starting at startTime.
  for (unsigned i = 0; i < m_events.size(); ++i) {
    if (m_events[i]->time() >= startTime) {
      m_events.remove(i, m_events.size() - i);
      break;
    }
  }
}

void AudioParamTimeline::cancelAndHoldAtTime(double cancelTime,
                                             ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (!isNonNegativeAudioParamTime(cancelTime, exceptionState))
    return;

  MutexLocker locker(m_eventsLock);

  unsigned i;
  // Find the first event at or just past cancelTime.
  for (i = 0; i < m_events.size(); ++i) {
    if (m_events[i]->time() > cancelTime) {
      break;
    }
  }

  // The event that is being cancelled.  This is the event just past
  // cancelTime, if any.
  unsigned cancelledEventIndex = i;

  // If the event just before cancelTime is a SetTarget or SetValueCurve
  // event, we need to handle that event specially instead of the event after.
  if (i > 0 && ((m_events[i - 1]->getType() == ParamEvent::SetTarget) ||
                (m_events[i - 1]->getType() == ParamEvent::SetValueCurve))) {
    cancelledEventIndex = i - 1;
  } else if (i >= m_events.size()) {
    // If there were no events occurring after |cancelTime| (and the
    // previous event is not SetTarget or SetValueCurve, we're done.
    return;
  }

  // cancelledEvent is the event that is being cancelled.
  ParamEvent* cancelledEvent = m_events[cancelledEventIndex].get();
  ParamEvent::Type eventType = cancelledEvent->getType();

  // New event to be inserted, if any, and a SetValueEvent if needed.
  std::unique_ptr<ParamEvent> newEvent = nullptr;
  std::unique_ptr<ParamEvent> newSetValueEvent = nullptr;

  switch (eventType) {
    case ParamEvent::LinearRampToValue:
    case ParamEvent::ExponentialRampToValue: {
      // For these events we need to remember the parameters of the event
      // for a CancelValues event so that we can properly cancel the event
      // and hold the value.
      std::unique_ptr<ParamEvent> savedEvent = ParamEvent::createGeneralEvent(
          eventType, cancelledEvent->value(), cancelledEvent->time(),
          cancelledEvent->initialValue(), cancelledEvent->callTime(),
          cancelledEvent->timeConstant(), cancelledEvent->duration(),
          cancelledEvent->curve(), cancelledEvent->curvePointsPerSecond(),
          cancelledEvent->curveEndValue(), nullptr);

      newEvent = ParamEvent::createCancelValuesEvent(cancelTime,
                                                     std::move(savedEvent));
    } break;
    case ParamEvent::SetTarget: {
      // Don't want to remove the SetTarget event, so bump the index.  But
      // we do want to insert a cancelEvent so that we stop this
      // automation and hold the value when we get there.
      ++cancelledEventIndex;

      newEvent = ParamEvent::createCancelValuesEvent(cancelTime, nullptr);
    } break;
    case ParamEvent::SetValueCurve: {
      double newDuration = cancelTime - cancelledEvent->time();

      if (cancelTime > cancelledEvent->time() + cancelledEvent->duration()) {
        // If the cancellation time is past the end of the curve,
        // there's nothing to do except remove the following events.
        ++cancelledEventIndex;
      } else {
        // Cancellation time is in the middle of the curve.  Therefore,
        // create a new SetValueCurve event with the appropriate new
        // parameters to cancel this event properly.  Since it's illegal
        // to insert any event within a SetValueCurve event, we can
        // compute the new end value now instead of doing when running
        // the timeline.
        float endValue = valueCurveAtTime(
            cancelTime, cancelledEvent->time(), cancelledEvent->duration(),
            cancelledEvent->curve().data(), cancelledEvent->curve().size());

        // Replace the existing SetValueCurve with this new one that is
        // identical except for the duration.
        newEvent = ParamEvent::createGeneralEvent(
            eventType, cancelledEvent->value(), cancelledEvent->time(),
            cancelledEvent->initialValue(), cancelledEvent->callTime(),
            cancelledEvent->timeConstant(), newDuration,
            cancelledEvent->curve(), cancelledEvent->curvePointsPerSecond(),
            endValue, nullptr);

        newSetValueEvent = ParamEvent::createSetValueEvent(
            endValue, cancelledEvent->time() + newDuration);
      }
    } break;
    case ParamEvent::SetValue:
    case ParamEvent::CancelValues:
      // Nothing needs to be done for a SetValue or CancelValues event.
      break;
    case ParamEvent::LastType:
      NOTREACHED();
      break;
  }

  // Now remove all the following events from the timeline.
  if (cancelledEventIndex < m_events.size())
    m_events.remove(cancelledEventIndex, m_events.size() - cancelledEventIndex);

  // Insert the new event, if any.
  if (newEvent) {
    insertEvent(std::move(newEvent), exceptionState);
    if (newSetValueEvent)
      insertEvent(std::move(newSetValueEvent), exceptionState);
  }
}

float AudioParamTimeline::valueForContextTime(
    AudioDestinationHandler& audioDestination,
    float defaultValue,
    bool& hasValue,
    float minValue,
    float maxValue) {
  {
    MutexTryLocker tryLocker(m_eventsLock);
    if (!tryLocker.locked() || !m_events.size() ||
        audioDestination.currentTime() < m_events[0]->time()) {
      hasValue = false;
      return defaultValue;
    }
  }

  // Ask for just a single value.
  float value;
  double sampleRate = audioDestination.sampleRate();
  size_t startFrame = audioDestination.currentSampleFrame();
  // One parameter change per render quantum.
  double controlRate = sampleRate / AudioUtilities::kRenderQuantumFrames;
  value = valuesForFrameRange(startFrame, startFrame + 1, defaultValue, &value,
                              1, sampleRate, controlRate, minValue, maxValue);

  hasValue = true;
  return value;
}

float AudioParamTimeline::valuesForFrameRange(size_t startFrame,
                                              size_t endFrame,
                                              float defaultValue,
                                              float* values,
                                              unsigned numberOfValues,
                                              double sampleRate,
                                              double controlRate,
                                              float minValue,
                                              float maxValue) {
  // We can't contend the lock in the realtime audio thread.
  MutexTryLocker tryLocker(m_eventsLock);
  if (!tryLocker.locked()) {
    if (values) {
      for (unsigned i = 0; i < numberOfValues; ++i)
        values[i] = defaultValue;
    }
    return defaultValue;
  }

  float lastValue =
      valuesForFrameRangeImpl(startFrame, endFrame, defaultValue, values,
                              numberOfValues, sampleRate, controlRate);

  // Clamp the values now to the nominal range
  for (unsigned k = 0; k < numberOfValues; ++k)
    values[k] = clampTo(values[k], minValue, maxValue);

  return lastValue;
}

float AudioParamTimeline::valuesForFrameRangeImpl(size_t startFrame,
                                                  size_t endFrame,
                                                  float defaultValue,
                                                  float* values,
                                                  unsigned numberOfValues,
                                                  double sampleRate,
                                                  double controlRate) {
  DCHECK(values);
  DCHECK_GE(numberOfValues, 1u);
  if (!values || !(numberOfValues >= 1))
    return defaultValue;

  // Return default value if there are no events matching the desired time
  // range.
  if (!m_events.size() || (endFrame / sampleRate <= m_events[0]->time())) {
    fillWithDefault(values, defaultValue, numberOfValues, 0);

    return defaultValue;
  }

  int numberOfEvents = m_events.size();

  if (numberOfEvents > 0) {
    double currentTime = startFrame / sampleRate;
    clampToCurrentTime(numberOfEvents, startFrame, sampleRate);

    if (handleAllEventsInThePast(currentTime, sampleRate, defaultValue,
                                 numberOfValues, values))
      return defaultValue;
  }

  // Maintain a running time (frame) and index for writing the values buffer.
  size_t currentFrame = startFrame;
  unsigned writeIndex = 0;

  // If first event is after startFrame then fill initial part of values buffer
  // with defaultValue until we reach the first event time.
  std::tie(currentFrame, writeIndex) =
      handleFirstEvent(values, defaultValue, numberOfValues, startFrame,
                       endFrame, sampleRate, currentFrame, writeIndex);

  float value = defaultValue;

  // Go through each event and render the value buffer where the times overlap,
  // stopping when we've rendered all the requested values.
  int lastSkippedEventIndex = 0;
  for (int i = 0; i < numberOfEvents && writeIndex < numberOfValues; ++i) {
    ParamEvent* event = m_events[i].get();
    ParamEvent* nextEvent = i < numberOfEvents - 1 ? m_events[i + 1].get() : 0;

    // Wait until we get a more recent event.
    if (!isEventCurrent(event, nextEvent, currentFrame, sampleRate)) {
      // This is not the special SetValue event case, and nextEvent is
      // in the past. We can skip processing of this event since it's
      // in past. We keep track of this event in lastSkippedEventIndex
      // to note what events we've skipped.
      lastSkippedEventIndex = i;
      continue;
    }

    // If there's no next event, set nextEventType to LastType to indicate that.
    ParamEvent::Type nextEventType =
        nextEvent ? static_cast<ParamEvent::Type>(nextEvent->getType())
                  : ParamEvent::LastType;

    processSetTargetFollowedByRamp(i, event, nextEventType, currentFrame,
                                   sampleRate, controlRate, value);

    float value1 = event->value();
    double time1 = event->time();

    float value2 = nextEvent ? nextEvent->value() : value1;
    double time2 = nextEvent ? nextEvent->time() : endFrame / sampleRate + 1;

    // Check to see if an event was cancelled.
    std::tie(value2, time2, nextEventType) =
        handleCancelValues(event, nextEvent, value2, time2);

    DCHECK_GE(time2, time1);

    // |fillToEndFrame| is the exclusive upper bound of the last frame to be
    // computed for this event.  It's either the last desired frame (|endFrame|)
    // or derived from the end time of the next event (time2). We compute
    // ceil(time2*sampleRate) because fillToEndFrame is the exclusive upper
    // bound.  Consider the case where |startFrame| = 128 and time2 = 128.1
    // (assuming sampleRate = 1).  Since time2 is greater than 128, we want to
    // output a value for frame 128.  This requires that fillToEndFrame be at
    // least 129.  This is achieved by ceil(time2).
    //
    // However, time2 can be very large, so compute this carefully in the case
    // where time2 exceeds the size of a size_t.

    size_t fillToEndFrame = endFrame;
    if (endFrame > time2 * sampleRate)
      fillToEndFrame = static_cast<size_t>(ceil(time2 * sampleRate));

    DCHECK_GE(fillToEndFrame, startFrame);
    size_t fillToFrame = fillToEndFrame - startFrame;
    fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));

    const AutomationState currentState = {
        numberOfValues, startFrame,     endFrame, sampleRate, controlRate,
        fillToFrame,    fillToEndFrame, value1,   time1,      value2,
        time2,          event,          i,
    };

    // First handle linear and exponential ramps which require looking ahead to
    // the next event.
    if (nextEventType == ParamEvent::LinearRampToValue) {
      std::tie(currentFrame, value, writeIndex) = processLinearRamp(
          currentState, values, currentFrame, value, writeIndex);
    } else if (nextEventType == ParamEvent::ExponentialRampToValue) {
      std::tie(currentFrame, value, writeIndex) = processExponentialRamp(
          currentState, values, currentFrame, value, writeIndex);
    } else {
      // Handle event types not requiring looking ahead to the next event.
      switch (event->getType()) {
        case ParamEvent::SetValue:
        case ParamEvent::LinearRampToValue: {
          currentFrame = fillToEndFrame;

          // Simply stay at a constant value.
          value = event->value();
          writeIndex = fillWithDefault(values, value, fillToFrame, writeIndex);

          break;
        }

        case ParamEvent::CancelValues: {
          std::tie(currentFrame, value, writeIndex) = processCancelValues(
              currentState, values, currentFrame, value, writeIndex);
          break;
        }

        case ParamEvent::ExponentialRampToValue: {
          currentFrame = fillToEndFrame;

          // If we're here, we've reached the end of the ramp.  If we can
          // (because the start and end values have the same sign, and neither
          // is 0), use the actual end value.  If not, we have to propagate
          // whatever we have.
          if (i >= 1 && ((m_events[i - 1]->value() * event->value()) > 0))
            value = event->value();

          // Simply stay at a constant value from the last time.  We don't want
          // to use the value of the event in case value1 * value2 < 0.  In this
          // case we should propagate the previous value, which is in |value|.
          writeIndex = fillWithDefault(values, value, fillToFrame, writeIndex);

          break;
        }

        case ParamEvent::SetTarget: {
          std::tie(currentFrame, value, writeIndex) = processSetTarget(
              currentState, values, currentFrame, value, writeIndex);
          break;
        }

        case ParamEvent::SetValueCurve: {
          std::tie(currentFrame, value, writeIndex) = processSetValueCurve(
              currentState, values, currentFrame, value, writeIndex);
          break;
        }
        case ParamEvent::LastType:
          ASSERT_NOT_REACHED();
          break;
      }
    }
  }

  // If we skipped over any events (because they are in the past), we can
  // remove them so we don't have to check them ever again.  (This MUST be
  // running with the m_events lock so we can safely modify the m_events
  // array.)
  if (lastSkippedEventIndex > 0)
    m_events.remove(0, lastSkippedEventIndex - 1);

  // If there's any time left after processing the last event then just
  // propagate the last value to the end of the values buffer.
  writeIndex = fillWithDefault(values, value, numberOfValues, writeIndex);

  // This value is used to set the .value attribute of the AudioParam.  it
  // should be the last computed value.
  return values[numberOfValues - 1];
}

std::tuple<size_t, unsigned> AudioParamTimeline::handleFirstEvent(
    float* values,
    float defaultValue,
    unsigned numberOfValues,
    size_t startFrame,
    size_t endFrame,
    double sampleRate,
    size_t currentFrame,
    unsigned writeIndex) {
  double firstEventTime = m_events[0]->time();
  if (firstEventTime > startFrame / sampleRate) {
    // |fillToFrame| is an exclusive upper bound, so use ceil() to compute the
    // bound from the firstEventTime.
    size_t fillToFrame = endFrame;
    double firstEventFrame = ceil(firstEventTime * sampleRate);
    if (endFrame > firstEventFrame)
      fillToFrame = static_cast<size_t>(firstEventFrame);
    DCHECK_GE(fillToFrame, startFrame);

    fillToFrame -= startFrame;
    fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));
    writeIndex = fillWithDefault(values, defaultValue, fillToFrame, writeIndex);

    currentFrame += fillToFrame;
  }

  return std::make_tuple(currentFrame, writeIndex);
}

bool AudioParamTimeline::isEventCurrent(const ParamEvent* event,
                                        const ParamEvent* nextEvent,
                                        size_t currentFrame,
                                        double sampleRate) {
  // WARNING: due to round-off it might happen that nextEvent->time() is
  // just larger than currentFrame/sampleRate.  This means that we will end
  // up running the |event| again.  The code below had better be prepared
  // for this case!  What should happen is the fillToFrame should be 0 so
  // that while the event is actually run again, nothing actually gets
  // computed, and we move on to the next event.
  //
  // An example of this case is setValueCurveAtTime.  The time at which
  // setValueCurveAtTime ends (and the setValueAtTime begins) might be
  // just past currentTime/sampleRate.  Then setValueCurveAtTime will be
  // processed again before advancing to setValueAtTime.  The number of
  // frames to be processed should be zero in this case.
  if (nextEvent && nextEvent->time() < currentFrame / sampleRate) {
    // But if the current event is a SetValue event and the event time is
    // between currentFrame - 1 and curentFrame (in time). we don't want to
    // skip it.  If we do skip it, the SetValue event is completely skipped
    // and not applied, which is wrong.  Other events don't have this problem.
    // (Because currentFrame is unsigned, we do the time check in this funny,
    // but equivalent way.)
    double eventFrame = event->time() * sampleRate;

    // Condition is currentFrame - 1 < eventFrame <= currentFrame, but
    // currentFrame is unsigned and could be 0, so use
    // currentFrame < eventFrame + 1 instead.
    if (!((event->getType() == ParamEvent::SetValue &&
           (eventFrame <= currentFrame) && (currentFrame < eventFrame + 1)))) {
      // This is not the special SetValue event case, and nextEvent is
      // in the past. We can skip processing of this event since it's
      // in past.
      return false;
    }
  }
  return true;
}

void AudioParamTimeline::clampToCurrentTime(int numberOfEvents,
                                            size_t startFrame,
                                            double sampleRate) {
  if (numberOfEvents > 0) {
    bool clampedSomeEventTime = false;
    double currentTime = startFrame / sampleRate;

    // Look at all the events in the timeline and check to see if any needs
    // to clamp the start time to the current time.
    for (int k = 0; k < numberOfEvents; ++k) {
      ParamEvent* event = m_events[k].get();

      // We're examining the event for the first time and the event time is
      // in the past so clamp the event time to the current time (start of
      // the rendering quantum).
      if (event->needsTimeClampCheck()) {
        if (event->time() < currentTime) {
          event->setTime(currentTime);
          clampedSomeEventTime = true;
        }

        // In all cases, we can clear the flag because the event is either
        // in the future, or we've already checked it (just now).
        event->clearTimeClampCheck();
      }
    }

    if (clampedSomeEventTime) {
      // If we clamped some event time to current time, we need to
      // sort the event list in time order again, but it must be
      // stable!
      std::stable_sort(m_events.begin(), m_events.end(),
                       ParamEvent::eventPreceeds);
    }
  }
}

bool AudioParamTimeline::handleAllEventsInThePast(double currentTime,
                                                  double sampleRate,
                                                  float defaultValue,
                                                  unsigned numberOfValues,
                                                  float* values) {
  // Optimize the case where the last event is in the past.
  ParamEvent* lastEvent = m_events[m_events.size() - 1].get();
  ParamEvent::Type lastEventType = lastEvent->getType();
  double lastEventTime = lastEvent->time();

  // If the last event is in the past and the event has ended, then we can
  // just propagate the same value.  Except for SetTarget which lasts
  // "forever".  SetValueCurve also has an explicit SetValue at the end of
  // the curve, so we don't need to worry that SetValueCurve time is a
  // start time, not an end time.
  if (lastEventTime + 1.5 * AudioUtilities::kRenderQuantumFrames / sampleRate <
          currentTime &&
      lastEventType != ParamEvent::SetTarget) {
    // The event has finished, so just copy the default value out.
    // Since all events are now also in the past, we can just remove all
    // timeline events too because |defaultValue| has the expected
    // value.
    fillWithDefault(values, defaultValue, numberOfValues, 0);
    m_smoothedValue = defaultValue;
    m_events.clear();
    return true;
  }

  return false;
}

void AudioParamTimeline::processSetTargetFollowedByRamp(
    int eventIndex,
    ParamEvent*& event,
    ParamEvent::Type nextEventType,
    size_t currentFrame,
    double sampleRate,
    double controlRate,
    float& value) {
  // If the current event is SetTarget and the next event is a
  // LinearRampToValue or ExponentialRampToValue, special handling is needed.
  // In this case, the linear and exponential ramp should start at wherever
  // the SetTarget processing has reached.
  if (event->getType() == ParamEvent::SetTarget &&
      (nextEventType == ParamEvent::LinearRampToValue ||
       nextEventType == ParamEvent::ExponentialRampToValue)) {
    // Replace the SetTarget with a SetValue to set the starting time and
    // value for the ramp using the current frame.  We need to update |value|
    // appropriately depending on whether the ramp has started or not.
    //
    // If SetTarget starts somewhere between currentFrame - 1 and
    // currentFrame, we directly compute the value it would have at
    // currentFrame.  If not, we update the value from the value from
    // currentFrame - 1.
    //
    // Can't use the condition currentFrame - 1 <= t0 * sampleRate <=
    // currentFrame because currentFrame is unsigned and could be 0.  Instead,
    // compute the condition this way,
    // where f = currentFrame and Fs = sampleRate:
    //
    //    f - 1 <= t0 * Fs <= f
    //    2 * f - 2 <= 2 * Fs * t0 <= 2 * f
    //    -2 <= 2 * Fs * t0 - 2 * f <= 0
    //    -1 <= 2 * Fs * t0 - 2 * f + 1 <= 1
    //     abs(2 * Fs * t0 - 2 * f + 1) <= 1
    if (fabs(2 * sampleRate * event->time() - 2 * currentFrame + 1) <= 1) {
      // SetTarget is starting somewhere between currentFrame - 1 and
      // currentFrame. Compute the value the SetTarget would have at the
      // currentFrame.
      value = event->value() +
              (value - event->value()) *
                  exp(-(currentFrame / sampleRate - event->time()) /
                      event->timeConstant());
    } else {
      // SetTarget has already started.  Update |value| one frame because it's
      // the value from the previous frame.
      float discreteTimeConstant =
          static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(
              event->timeConstant(), controlRate));
      value += (event->value() - value) * discreteTimeConstant;
    }

    // Insert a SetValueEvent to mark the starting value and time.
    // Clear the clamp check because this doesn't need it.
    m_events[eventIndex] =
        ParamEvent::createSetValueEvent(value, currentFrame / sampleRate);
    m_events[eventIndex]->clearTimeClampCheck();

    // Update our pointer to the current event because we just changed it.
    event = m_events[eventIndex].get();
  }
}

std::tuple<float, double, AudioParamTimeline::ParamEvent::Type>
AudioParamTimeline::handleCancelValues(const ParamEvent* currentEvent,
                                       ParamEvent* nextEvent,
                                       float value2,
                                       double time2) {
  DCHECK(currentEvent);

  ParamEvent::Type nextEventType =
      nextEvent ? nextEvent->getType() : ParamEvent::LastType;

  if (nextEvent && nextEvent->getType() == ParamEvent::CancelValues) {
    float value1 = currentEvent->value();
    double time1 = currentEvent->time();

    switch (currentEvent->getType()) {
      case ParamEvent::LinearRampToValue:
      case ParamEvent::ExponentialRampToValue:
      case ParamEvent::SetValue: {
        // These three events potentially establish a starting value for
        // the following event, so we need to examine the cancelled
        // event to see what to do.
        const ParamEvent* savedEvent = nextEvent->savedEvent();

        // Update the end time and type to pretend that we're running
        // this saved event type.
        time2 = nextEvent->time();
        nextEventType = savedEvent->getType();

        if (nextEvent->hasDefaultCancelledValue()) {
          // We've already established a value for the cancelled
          // event, so just return it.
          value2 = nextEvent->value();
        } else {
          // If the next event would have been a LinearRamp or
          // ExponentialRamp, we need to compute a new end value for
          // the event so that the curve works continues as if it were
          // not cancelled.
          switch (savedEvent->getType()) {
            case ParamEvent::LinearRampToValue:
              value2 =
                  linearRampAtTime(nextEvent->time(), value1, time1,
                                   savedEvent->value(), savedEvent->time());
              break;
            case ParamEvent::ExponentialRampToValue:
              value2 = exponentialRampAtTime(nextEvent->time(), value1, time1,
                                             savedEvent->value(),
                                             savedEvent->time());
              break;
            case ParamEvent::SetValueCurve:
            case ParamEvent::SetValue:
            case ParamEvent::SetTarget:
            case ParamEvent::CancelValues:
              // These cannot be possible types for the saved event
              // because they can't be created.
              // createCancelValuesEvent doesn't allow them (SetValue,
              // SetTarget, CancelValues) or cancelScheduledValues()
              // doesn't create such an event (SetValueCurve).
              NOTREACHED();
              break;
            case ParamEvent::LastType:
              // Illegal event type.
              NOTREACHED();
              break;
          }

          // Cache the new value so we don't keep computing it over and over.
          nextEvent->setCancelledValue(value2);
        }
      } break;
      case ParamEvent::SetValueCurve:
        // Everything needed for this was handled when cancelling was
        // done.
        break;
      case ParamEvent::SetTarget:
      case ParamEvent::CancelValues:
        // Nothing special needs to be done for SetTarget or
        // CancelValues followed by CancelValues.
        break;
      case ParamEvent::LastType:
        NOTREACHED();
        break;
    }
  }

  return std::make_tuple(value2, time2, nextEventType);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::processLinearRamp(
    const AutomationState& currentState,
    float* values,
    size_t currentFrame,
    float value,
    unsigned writeIndex) {
#if CPU(X86) || CPU(X86_64)
  auto numberOfValues = currentState.numberOfValues;
#endif
  auto fillToFrame = currentState.fillToFrame;
  auto time1 = currentState.time1;
  auto time2 = currentState.time2;
  auto value1 = currentState.value1;
  auto value2 = currentState.value2;
  auto sampleRate = currentState.sampleRate;

  double deltaTime = time2 - time1;
  float k = deltaTime > 0 ? 1 / deltaTime : 0;
  const float valueDelta = value2 - value1;
#if CPU(X86) || CPU(X86_64)
  if (fillToFrame > writeIndex) {
    // Minimize in-loop operations. Calculate starting value and increment.
    // Next step: value += inc.
    //  value = value1 +
    //      (currentFrame/sampleRate - time1) * k * (value2 - value1);
    //  inc = 4 / sampleRate * k * (value2 - value1);
    // Resolve recursion by expanding constants to achieve a 4-step loop
    // unrolling.
    //  value = value1 +
    //    ((currentFrame/sampleRate - time1) + i * sampleFrameTimeIncr) * k
    //    * (value2 -value1), i in 0..3
    __m128 vValue =
        _mm_mul_ps(_mm_set_ps1(1 / sampleRate), _mm_set_ps(3, 2, 1, 0));
    vValue = _mm_add_ps(vValue, _mm_set_ps1(currentFrame / sampleRate - time1));
    vValue = _mm_mul_ps(vValue, _mm_set_ps1(k * valueDelta));
    vValue = _mm_add_ps(vValue, _mm_set_ps1(value1));
    __m128 vInc = _mm_set_ps1(4 / sampleRate * k * valueDelta);

    // Truncate loop steps to multiple of 4.
    unsigned fillToFrameTrunc =
        writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
    // Compute final time.
    DCHECK_LE(fillToFrameTrunc, numberOfValues);
    currentFrame += fillToFrameTrunc - writeIndex;

    // Process 4 loop steps.
    for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
      _mm_storeu_ps(values + writeIndex, vValue);
      vValue = _mm_add_ps(vValue, vInc);
    }
  }
  // Update |value| with the last value computed so that the
  // .value attribute of the AudioParam gets the correct linear
  // ramp value, in case the following loop doesn't execute.
  if (writeIndex >= 1)
    value = values[writeIndex - 1];
#endif
  // Serially process remaining values.
  for (; writeIndex < fillToFrame; ++writeIndex) {
    float x = (currentFrame / sampleRate - time1) * k;
    // value = (1 - x) * value1 + x * value2;
    value = value1 + x * valueDelta;
    values[writeIndex] = value;
    ++currentFrame;
  }

  return std::make_tuple(currentFrame, value, writeIndex);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::processExponentialRamp(
    const AutomationState& currentState,
    float* values,
    size_t currentFrame,
    float value,
    unsigned writeIndex) {
  auto fillToFrame = currentState.fillToFrame;
  auto time1 = currentState.time1;
  auto time2 = currentState.time2;
  auto value1 = currentState.value1;
  auto value2 = currentState.value2;
  auto sampleRate = currentState.sampleRate;

  if (value1 * value2 <= 0) {
    // It's an error if value1 and value2 have opposite signs or if one of
    // them is zero.  Handle this by propagating the previous value, and
    // making it the default.
    value = value1;

    for (; writeIndex < fillToFrame; ++writeIndex)
      values[writeIndex] = value;
  } else {
    double deltaTime = time2 - time1;
    double numSampleFrames = deltaTime * sampleRate;
    // The value goes exponentially from value1 to value2 in a duration of
    // deltaTime seconds according to
    //
    //  v(t) = v1*(v2/v1)^((t-t1)/(t2-t1))
    //
    // Let c be currentFrame and F be the sampleRate.  Then we want to
    // sample v(t) at times t = (c + k)/F for k = 0, 1, ...:
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
    // |value| got updated one extra time in the above loop.  Restore it to
    // the last computed value.
    if (writeIndex >= 1)
      value /= multiplier;

    // Due to roundoff it's possible that value exceeds value2.  Clip value
    // to value2 if we are within 1/2 frame of time2.
    if (currentFrame > time2 * sampleRate - 0.5)
      value = value2;
  }

  return std::make_tuple(currentFrame, value, writeIndex);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::processSetTarget(
    const AutomationState& currentState,
    float* values,
    size_t currentFrame,
    float value,
    unsigned writeIndex) {
#if CPU(X86) || CPU(X86_64)
  auto numberOfValues = currentState.numberOfValues;
#endif
  auto fillToFrame = currentState.fillToFrame;
  auto time1 = currentState.time1;
  auto value1 = currentState.value1;
  auto sampleRate = currentState.sampleRate;
  auto controlRate = currentState.controlRate;
  auto fillToEndFrame = currentState.fillToEndFrame;
  auto event = currentState.event;

  // Exponential approach to target value with given time constant.
  //
  //   v(t) = v2 + (v1 - v2)*exp(-(t-t1/tau))
  //
  float target = value1;
  float timeConstant = event->timeConstant();
  float discreteTimeConstant =
      static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(
          timeConstant, controlRate));

  // Set the starting value correctly.  This is only needed when the
  // current time is "equal" to the start time of this event.  This is
  // to get the sampling correct if the start time of this automation
  // isn't on a frame boundary.  Otherwise, we can just continue from
  // where we left off from the previous rendering quantum.
  {
    double rampStartFrame = time1 * sampleRate;
    // Condition is c - 1 < r <= c where c = currentFrame and r =
    // rampStartFrame.  Compute it this way because currentFrame is
    // unsigned and could be 0.
    if (rampStartFrame <= currentFrame && currentFrame < rampStartFrame + 1) {
      value = target +
              (value - target) *
                  exp(-(currentFrame / sampleRate - time1) / timeConstant);
    } else {
      // Otherwise, need to compute a new value bacause |value| is the
      // last computed value of SetTarget.  Time has progressed by one
      // frame, so we need to update the value for the new frame.
      value += (target - value) * discreteTimeConstant;
    }
  }

  // If the value is close enough to the target, just fill in the data
  // with the target value.
  if (fabs(value - target) < kSetTargetThreshold * fabs(target) ||
      (!target && fabs(value) < kSetTargetZeroThreshold)) {
    for (; writeIndex < fillToFrame; ++writeIndex)
      values[writeIndex] = target;
  } else {
#if CPU(X86) || CPU(X86_64)
    if (fillToFrame > writeIndex) {
      // Resolve recursion by expanding constants to achieve a 4-step
      // loop unrolling.
      //
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
      unsigned fillToFrameTrunc =
          writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
      DCHECK_LE(fillToFrameTrunc, numberOfValues);

      for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
        delta = target - value;
        vDelta = _mm_set_ps1(delta);
        vValue = _mm_set_ps1(value);

        vResult = _mm_add_ps(vValue, _mm_mul_ps(vDelta, vC));
        _mm_storeu_ps(values + writeIndex, vResult);

        // Update value for next iteration.
        value += delta * c3;
      }
    }
#endif
    // Serially process remaining values
    for (; writeIndex < fillToFrame; ++writeIndex) {
      values[writeIndex] = value;
      value += (target - value) * discreteTimeConstant;
    }
    // The previous loops may have updated |value| one extra time.
    // Reset it to the last computed value.
    if (writeIndex >= 1)
      value = values[writeIndex - 1];
    currentFrame = fillToEndFrame;
  }

  return std::make_tuple(currentFrame, value, writeIndex);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::processSetValueCurve(
    const AutomationState& currentState,
    float* values,
    size_t currentFrame,
    float value,
    unsigned writeIndex) {
  auto numberOfValues = currentState.numberOfValues;
  auto fillToFrame = currentState.fillToFrame;
  auto time1 = currentState.time1;
  auto sampleRate = currentState.sampleRate;
  auto startFrame = currentState.startFrame;
  auto endFrame = currentState.endFrame;
  auto fillToEndFrame = currentState.fillToEndFrame;
  auto event = currentState.event;

  const Vector<float> curve = event->curve();
  const float* curveData = curve.data();
  unsigned numberOfCurvePoints = curve.size();

  float curveEndValue = event->curveEndValue();

  // Curve events have duration, so don't just use next event time.
  double duration = event->duration();
  // How much to step the curve index for each frame.  This is basically
  // the term (N - 1)/Td in the specification.
  double curvePointsPerFrame = event->curvePointsPerSecond() / sampleRate;

  if (!numberOfCurvePoints || duration <= 0 || sampleRate <= 0) {
    // Error condition - simply propagate previous value.
    currentFrame = fillToEndFrame;
    for (; writeIndex < fillToFrame; ++writeIndex)
      values[writeIndex] = value;
    return std::make_tuple(currentFrame, value, writeIndex);
  }

  // Save old values and recalculate information based on the curve's
  // duration instead of the next event time.
  size_t nextEventFillToFrame = fillToFrame;

  // fillToEndFrame = min(endFrame,
  //                      ceil(sampleRate * (time1 + duration))),
  // but compute this carefully in case sampleRate*(time1 + duration) is
  // huge.  fillToEndFrame is an exclusive upper bound of the last frame
  // to be computed, so ceil is used.
  {
    double curveEndFrame = ceil(sampleRate * (time1 + duration));
    if (endFrame > curveEndFrame)
      fillToEndFrame = static_cast<size_t>(curveEndFrame);
    else
      fillToEndFrame = endFrame;
  }

  // |fillToFrame| can be less than |startFrame| when the end of the
  // setValueCurve automation has been reached, but the next automation
  // has not yet started. In this case, |fillToFrame| is clipped to
  // |time1|+|duration| above, but |startFrame| will keep increasing
  // (because the current time is increasing).
  fillToFrame = (fillToEndFrame < startFrame) ? 0 : fillToEndFrame - startFrame;
  fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));

  // Index into the curve data using a floating-point value.
  // We're scaling the number of curve points by the duration (see
  // curvePointsPerFrame).
  double curveVirtualIndex = 0;
  if (time1 < currentFrame / sampleRate) {
    // Index somewhere in the middle of the curve data.
    // Don't use timeToSampleFrame() since we want the exact
    // floating-point frame.
    double frameOffset = currentFrame - time1 * sampleRate;
    curveVirtualIndex = curvePointsPerFrame * frameOffset;
  }

  // Set the default value in case fillToFrame is 0.
  value = curveEndValue;

  // Render the stretched curve data using linear interpolation.
  // Oversampled curve data can be provided if sharp discontinuities are
  // desired.
  unsigned k = 0;
#if CPU(X86) || CPU(X86_64)
  if (fillToFrame > writeIndex) {
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
    DCHECK_LE(fillToFrameTrunc, numberOfValues);

    for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
      // Compute current index this way to minimize round-off that would
      // have occurred by incrementing the index by curvePointsPerFrame.
      __m128 vCurrentVirtualIndex =
          _mm_add_ps(vCurveVirtualIndex, _mm_mul_ps(vK, vCurvePointsPerFrame));
      vK = _mm_add_ps(vK, vN4);

      // Clamp index to the last element of the array.
      __m128i vCurveIndex0 = _mm_cvttps_epi32(
          _mm_min_ps(vCurrentVirtualIndex, vNumberOfCurvePointsM1));
      __m128i vCurveIndex1 = _mm_cvttps_epi32(_mm_min_ps(
          _mm_add_ps(vCurrentVirtualIndex, vN1), vNumberOfCurvePointsM1));

      // Linearly interpolate between the two nearest curve points.
      // |delta| is clamped to 1 because currentVirtualIndex can exceed
      // curveIndex0 by more than one.  This can happen when we reached
      // the end of the curve but still need values to fill out the
      // current rendering quantum.
      _mm_storeu_si128((__m128i*)aCurveIndex0, vCurveIndex0);
      _mm_storeu_si128((__m128i*)aCurveIndex1, vCurveIndex1);
      __m128 vC0 =
          _mm_set_ps(curveData[aCurveIndex0[3]], curveData[aCurveIndex0[2]],
                     curveData[aCurveIndex0[1]], curveData[aCurveIndex0[0]]);
      __m128 vC1 =
          _mm_set_ps(curveData[aCurveIndex1[3]], curveData[aCurveIndex1[2]],
                     curveData[aCurveIndex1[1]], curveData[aCurveIndex1[0]]);
      __m128 vDelta = _mm_min_ps(
          _mm_sub_ps(vCurrentVirtualIndex, _mm_cvtepi32_ps(vCurveIndex0)), vN1);

      __m128 vValue = _mm_add_ps(vC0, _mm_mul_ps(_mm_sub_ps(vC1, vC0), vDelta));

      _mm_storeu_ps(values + writeIndex, vValue);
    }
    // Pass along k to the serial loop.
    k = truncatedSteps;
  }
  if (writeIndex >= 1)
    value = values[writeIndex - 1];
#endif
  for (; writeIndex < fillToFrame; ++writeIndex, ++k) {
    // Compute current index this way to minimize round-off that would
    // have occurred by incrementing the index by curvePointsPerFrame.
    double currentVirtualIndex = curveVirtualIndex + k * curvePointsPerFrame;
    unsigned curveIndex0;

    // Clamp index to the last element of the array.
    if (currentVirtualIndex < numberOfCurvePoints) {
      curveIndex0 = static_cast<unsigned>(currentVirtualIndex);
    } else {
      curveIndex0 = numberOfCurvePoints - 1;
    }

    unsigned curveIndex1 = std::min(curveIndex0 + 1, numberOfCurvePoints - 1);

    // Linearly interpolate between the two nearest curve points.
    // |delta| is clamped to 1 because currentVirtualIndex can exceed
    // curveIndex0 by more than one.  This can happen when we reached
    // the end of the curve but still need values to fill out the
    // current rendering quantum.
    DCHECK_LT(curveIndex0, numberOfCurvePoints);
    DCHECK_LT(curveIndex1, numberOfCurvePoints);
    float c0 = curveData[curveIndex0];
    float c1 = curveData[curveIndex1];
    double delta = std::min(currentVirtualIndex - curveIndex0, 1.0);

    value = c0 + (c1 - c0) * delta;

    values[writeIndex] = value;
  }

  // If there's any time left after the duration of this event and the
  // start of the next, then just propagate the last value of the
  // curveData. Don't modify |value| unless there is time left.
  if (writeIndex < nextEventFillToFrame) {
    value = curveEndValue;
    for (; writeIndex < nextEventFillToFrame; ++writeIndex)
      values[writeIndex] = value;
  }

  // Re-adjust current time
  currentFrame += nextEventFillToFrame;

  return std::make_tuple(currentFrame, value, writeIndex);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::processCancelValues(
    const AutomationState& currentState,
    float* values,
    size_t currentFrame,
    float value,
    unsigned writeIndex) {
  auto fillToFrame = currentState.fillToFrame;
  auto time1 = currentState.time1;
  auto sampleRate = currentState.sampleRate;
  auto controlRate = currentState.controlRate;
  auto fillToEndFrame = currentState.fillToEndFrame;
  auto event = currentState.event;
  auto eventIndex = currentState.eventIndex;

  // If the previous event was a SetTarget or ExponentialRamp
  // event, the current value is one sample behind.  Update
  // the sample value by one sample, but only at the start of
  // this CancelValues event.
  if (event->hasDefaultCancelledValue()) {
    value = event->value();
  } else {
    double cancelFrame = time1 * sampleRate;
    if (eventIndex >= 1 && cancelFrame <= currentFrame &&
        currentFrame < cancelFrame + 1) {
      ParamEvent::Type lastEventType = m_events[eventIndex - 1]->getType();
      if (lastEventType == ParamEvent::SetTarget) {
        float target = m_events[eventIndex - 1]->value();
        float timeConstant = m_events[eventIndex - 1]->timeConstant();
        float discreteTimeConstant = static_cast<float>(
            AudioUtilities::discreteTimeConstantForSampleRate(timeConstant,
                                                              controlRate));
        value += (target - value) * discreteTimeConstant;
      }
    }
  }

  // Simply stay at the current value.
  for (; writeIndex < fillToFrame; ++writeIndex)
    values[writeIndex] = value;

  currentFrame = fillToEndFrame;

  return std::make_tuple(currentFrame, value, writeIndex);
}

unsigned AudioParamTimeline::fillWithDefault(float* values,
                                             float defaultValue,
                                             size_t endFrame,
                                             unsigned writeIndex) {
  size_t index = writeIndex;

  for (; index < endFrame; ++index)
    values[index] = defaultValue;

  return index;
}

}  // namespace blink
