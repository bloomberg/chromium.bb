// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ViewportScrollCallback.h"

#include "core/page/scrolling/ScrollState.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

ScrollResult ViewportScrollCallback::performNativeScroll(
    ScrollState& state, ScrollableArea& scroller)
{
    FloatSize delta(state.deltaX(), state.deltaY());
    ScrollGranularity granularity =
        ScrollGranularity(static_cast<int>(state.deltaGranularity()));

    ScrollResult result = scroller.userScroll(granularity, delta);

    // The viewport consumes everything.
    // TODO(bokan): This isn't actually consuming everything but doing so breaks
    // the main thread pull-to-refresh action. crbug.com/607210.
    state.consumeDeltaNative(
        delta.width() - result.unusedScrollDeltaX,
        delta.height() - result.unusedScrollDeltaY);

    return result;
}

} // namespace blink
