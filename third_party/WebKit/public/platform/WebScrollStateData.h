// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScrollStateData_h
#define WebScrollStateData_h

#include "WebCommon.h"

#include <deque>

namespace blink {

// WebScrollState data contains the data used by blink::ScrollState. This is
// used by the scroll customization API, detailed here (https://goo.gl/1ipTpP).
// For the cc equivalent, see cc::ScrollStateData and cc::ScrollState.
struct BLINK_EXPORT WebScrollStateData {
    WebScrollStateData(double deltaX, double deltaY, double deltaGranularity,
        double velocityX, double velocityY, bool inInertialPhase,
        bool isBeginning = false, bool isEnding = false,
        bool fromUserInput = false, bool shouldPropagate = true,
        bool deltaConsumedForScrollSequence = false)
        : deltaX(deltaX)
        , deltaY(deltaY)
        , deltaGranularity(deltaGranularity)
        , velocityX(velocityX)
        , velocityY(velocityY)
        , inInertialPhase(inInertialPhase)
        , isBeginning(isBeginning)
        , isEnding(isEnding)
        , fromUserInput(fromUserInput)
        , shouldPropagate(shouldPropagate)
        , currentNativeScrollingElement(0)
        , deltaConsumedForScrollSequence(deltaConsumedForScrollSequence)
        , causedScrollX(false)
        , causedScrollY(false)
    {
    }

    WebScrollStateData()
        : WebScrollStateData(0, 0, 0, 0, 0, false)
    {
    }

    double deltaX;
    double deltaY;
    double deltaGranularity;
    double velocityX;
    double velocityY;
    bool inInertialPhase;
    bool isBeginning;
    bool isEnding;

    bool fromUserInput;
    bool shouldPropagate;
    // The id of the last native element to respond to a scroll, or 0 if none exists.
    int currentNativeScrollingElement;
    // Whether the scroll sequence has had any delta consumed, in the
    // current frame, or any child frames.
    bool deltaConsumedForScrollSequence;

    bool causedScrollX;
    bool causedScrollY;
};

} // namespace blink

#endif // WebScrollStateData_h
