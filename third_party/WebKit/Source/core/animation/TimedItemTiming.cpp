// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/TimedItemTiming.h"

#include "core/animation/Animation.h"
#include "core/animation/TimedItem.h"
#include "platform/animation/TimingFunction.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<TimedItemTiming> TimedItemTiming::create(TimedItem* parent)
{
    return adoptRefWillBeNoop(new TimedItemTiming(parent));
}

TimedItemTiming::TimedItemTiming(TimedItem* parent)
: m_parent(parent)
{
}

double TimedItemTiming::delay()
{
    return m_parent->specifiedTiming().startDelay * 1000;
}

double TimedItemTiming::endDelay()
{
    return m_parent->specifiedTiming().endDelay * 1000;
}

String TimedItemTiming::fill()
{
    Timing::FillMode fillMode = m_parent->specifiedTiming().fillMode;
    switch (fillMode) {
    case Timing::FillModeNone:
        return "none";
    case Timing::FillModeForwards:
        return "forwards";
    case Timing::FillModeBackwards:
        return "backwards";
    case Timing::FillModeBoth:
        return "both";
    case Timing::FillModeAuto:
        return "auto";
    }
    ASSERT_NOT_REACHED();
    return "auto";
}

double TimedItemTiming::iterationStart()
{
    return m_parent->specifiedTiming().iterationStart;
}

double TimedItemTiming::iterations()
{
    return m_parent->specifiedTiming().iterationCount;
}

// This logic was copied from the example in bindings/tests/idls/TestInterface.idl
// and bindings/tests/results/V8TestInterface.cpp.
// FIXME: It might be possible to have 'duration' defined as an attribute in the idl.
// If possible, fix will be in a follow-up patch.
void TimedItemTiming::getDuration(String propertyName, bool& element0Enabled, double& element0, bool& element1Enabled, String& element1)
{
    if (propertyName != "duration")
        return;

    if (std::isnan(m_parent->specifiedTiming().iterationDuration)) {
        element1Enabled = true;
        element1 = "auto";
        return;
    }
    element0Enabled = true;
    element0 = m_parent->specifiedTiming().iterationDuration * 1000;
    return;
}

double TimedItemTiming::playbackRate()
{
    return m_parent->specifiedTiming().playbackRate;
}

String TimedItemTiming::direction()
{
    Timing::PlaybackDirection direction = m_parent->specifiedTiming().direction;
    switch (direction) {
    case Timing::PlaybackDirectionNormal:
        return "normal";
    case Timing::PlaybackDirectionReverse:
        return "reverse";
    case Timing::PlaybackDirectionAlternate:
        return "alternate";
    case Timing::PlaybackDirectionAlternateReverse:
        return "alternate-reverse";
    }
    ASSERT_NOT_REACHED();
    return "normal";
}

String TimedItemTiming::easing()
{
    return m_parent->specifiedTiming().timingFunction->toString();
}

void TimedItemTiming::setDelay(double delay)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setStartDelay(timing, delay);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::setEndDelay(double endDelay)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setEndDelay(timing, endDelay);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::setFill(String fill)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setFillMode(timing, fill);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::setIterationStart(double iterationStart)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setIterationStart(timing, iterationStart);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::setIterations(double iterations)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setIterationCount(timing, iterations);
    m_parent->updateSpecifiedTiming(timing);
}

bool TimedItemTiming::setDuration(String name, double duration)
{
    if (name != "duration")
        return false;
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setIterationDuration(timing, duration);
    m_parent->updateSpecifiedTiming(timing);
    return true;
}

void TimedItemTiming::setPlaybackRate(double playbackRate)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setPlaybackRate(timing, playbackRate);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::setDirection(String direction)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setPlaybackDirection(timing, direction);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::setEasing(String easing)
{
    Timing timing = m_parent->specifiedTiming();
    TimingInput::setTimingFunction(timing, easing);
    m_parent->updateSpecifiedTiming(timing);
}

void TimedItemTiming::trace(Visitor* visitor)
{
    visitor->trace(m_parent);
}

} // namespace WebCore
