// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollState_h
#define ScrollState_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Element.h"
#include "wtf/Vector.h"

namespace blink {

class ScrollState final : public RefCountedWillBeGarbageCollected<ScrollState>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static PassRefPtrWillBeRawPtr<ScrollState> create(
        double deltaX, double deltaY, double deltaGranularity, double velocityX,
        double velocityY, bool inInertialPhase, bool isEnding, bool fromUserInput = false,
        bool shouldPropagate = true, bool deltaConsumedForScrollSequence = false);

    // Web exposed methods.

    // Reduce deltas by x, y.
    void consumeDelta(double x, double y, ExceptionState&);
    // Positive when scrolling left.
    double deltaX() const { return m_deltaX; }
    // Positive when scrolling up.
    double deltaY() const { return m_deltaY; }
    // Indicates the smallest delta the input device can produce. 0 for unquantized inputs.
    double deltaGranularity() const { return m_deltaGranularity; }
    // Positive if moving right.
    double velocityX() const { return m_velocityX; }
    // Positive if moving down.
    double velocityY() const { return m_velocityY; }
    // True for events dispatched after the users's gesture has finished.
    bool inInertialPhase() const { return m_inInertialPhase; }
    // True if this is the last event for this scroll.
    bool isEnding() const { return m_isEnding; }
    // True if this scroll is the direct result of user input.
    bool fromUserInput() const { return m_fromUserInput; }
    // True if this scroll is allowed to bubble upwards.
    bool shouldPropagate() const { return m_shouldPropagate; }

    // Non web exposed methods.
    void consumeDeltaNative(double x, double y);

    void setCurrentNativeScrollingElement(Element* element)
    {
        m_currentNativeScrollingElement = element;
    }

    Element* currentNativeScrollingElement() const
    {
        return m_currentNativeScrollingElement.get();
    }

    void setDeltaConsumedForScrollSequence(bool deltaConsumedForScrollSequence)
    {
        m_deltaConsumedForScrollSequence = deltaConsumedForScrollSequence;
    }

    bool deltaConsumedForScrollSequence() const
    {
        return m_deltaConsumedForScrollSequence;
    }

    bool fullyConsumed() const
    {
        return !m_deltaX && !m_deltaY && !m_isEnding;
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_currentNativeScrollingElement);
    };

private:
    ScrollState();
    ScrollState(double deltaX, double deltaY, double deltaGranularity,
        double velocityX, double velocityY, bool inInertialPhase, bool isEnding,
        bool fromUserInput, bool shouldPropagate, bool deltaConsumedForScrollSequence);

    double m_deltaX;
    double m_deltaY;
    double m_deltaGranularity;
    double m_velocityX;
    double m_velocityY;
    bool m_inInertialPhase;
    bool m_isEnding;

    bool m_fromUserInput;
    bool m_shouldPropagate;
    // The last native element to respond to a scroll, or null if none exists.
    RefPtrWillBeMember<Element> m_currentNativeScrollingElement;
    // Whether the scroll sequence has had any delta consumed.
    bool m_deltaConsumedForScrollSequence;
};

} // namespace blink

#endif // ScrollState_h
