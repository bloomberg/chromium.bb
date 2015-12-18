// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollState_h
#define ScrollState_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "public/platform/WebScrollStateData.h"

namespace blink {

class CORE_EXPORT ScrollState final : public RefCountedWillBeGarbageCollectedFinalized<ScrollState>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static PassRefPtrWillBeRawPtr<ScrollState> create(
        double deltaX, double deltaY, double deltaGranularity, double velocityX,
        double velocityY, bool inInertialPhase,
        bool isBeginning = false, bool isEnding = false,
        bool fromUserInput = false, bool shouldPropagate = true,
        bool deltaConsumedForScrollSequence = false);

    static PassRefPtrWillBeRawPtr<ScrollState> create(PassOwnPtr<WebScrollStateData>);

    ~ScrollState()
    {
    }

    // Web exposed methods.

    // Reduce deltas by x, y.
    void consumeDelta(double x, double y, ExceptionState&);
    // Pops the first element off of |m_scrollChain| and calls |distributeScroll| on it.
    void distributeToScrollChainDescendant();
    // Positive when scrolling left.
    double deltaX() const { return m_data->deltaX; };
    // Positive when scrolling up.
    double deltaY() const { return m_data->deltaY; };
    // Indicates the smallest delta the input device can produce. 0 for unquantized inputs.
    double deltaGranularity() const { return m_data->deltaGranularity; };
    // Positive if moving right.
    double velocityX() const { return m_data->velocityX; };
    // Positive if moving down.
    double velocityY() const { return m_data->velocityY; };
    // True for events dispatched after the users's gesture has finished.
    bool inInertialPhase() const { return m_data->inInertialPhase; };
    // True if this is the first event for this scroll.
    bool isBeginning() const { return m_data->isBeginning; };
    // True if this is the last event for this scroll.
    bool isEnding() const { return m_data->isEnding; };
    // True if this scroll is the direct result of user input.
    bool fromUserInput() const { return m_data->fromUserInput; };
    // True if this scroll is allowed to bubble upwards.
    bool shouldPropagate() const { return m_data->shouldPropagate; };

    // Non web exposed methods.
    void consumeDeltaNative(double x, double y);

    // TODO(tdresser): this needs to be web exposed. See crbug.com/483091.
    void setScrollChain(std::deque<int> scrollChain)
    {
        m_scrollChain = scrollChain;
    }

    Element* currentNativeScrollingElement() const;
    void setCurrentNativeScrollingElement(Element*);

    int currentNativeScrollingElementId() const;
    void setCurrentNativeScrollingElementById(int elementId);

    bool deltaConsumedForScrollSequence() const
    {
        return m_data->deltaConsumedForScrollSequence;
    }

    // Scroll begin and end must propagate to all nodes to ensure
    // their state is updated.
    bool fullyConsumed() const
    {
        return !m_data->deltaX && !m_data->deltaY && !m_data->isEnding && !m_data->isBeginning;
    }

    // These are only used for CompositorWorker scrolling, and should be gotten
    // rid of eventually.
    void setCausedScroll(bool x, bool y)
    {
        m_data->causedScrollX = x;
        m_data->causedScrollY = y;
    }

    bool causedScrollX() const
    {
        return m_data->causedScrollX;
    }

    bool causedScrollY() const
    {
        return m_data->causedScrollY;
    }

    WebScrollStateData* data() const { return m_data.get(); }

    DEFINE_INLINE_TRACE() {}

private:
    ScrollState();
    ScrollState(PassOwnPtr<WebScrollStateData>);
    PassOwnPtr<WebScrollStateData> m_data;
    std::deque<int> m_scrollChain;
};

} // namespace blink

#endif // ScrollState_h
