// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/EventHandlingUtil.h"

#include "core/frame/FrameView.h"

namespace blink {
namespace EventHandlingUtil {

HitTestResult hitTestResultInFrame(LocalFrame* frame, const LayoutPoint& point, HitTestRequest::HitTestRequestType hitType)
{
    HitTestResult result(HitTestRequest(hitType), point);

    if (!frame || frame->contentLayoutItem().isNull())
        return result;
    if (frame->view()) {
        IntRect rect = frame->view()->visibleContentRect(IncludeScrollbars);
        if (!rect.contains(roundedIntPoint(point)))
            return result;
    }
    frame->contentLayoutItem().hitTest(result);
    return result;
}

WebInputEventResult mergeEventResult(
    WebInputEventResult resultA, WebInputEventResult resultB)
{
    // The ordering of the enumeration is specific. There are times that
    // multiple events fire and we need to combine them into a single
    // result code. The enumeration is based on the level of consumption that
    // is most significant. The enumeration is ordered with smaller specified
    // numbers first. Examples of merged results are:
    // (HandledApplication, HandledSystem) -> HandledSystem
    // (NotHandled, HandledApplication) -> HandledApplication
    static_assert(static_cast<int>(WebInputEventResult::NotHandled) == 0, "WebInputEventResult not ordered");
    static_assert(static_cast<int>(WebInputEventResult::HandledSuppressed) < static_cast<int>(WebInputEventResult::HandledApplication), "WebInputEventResult not ordered");
    static_assert(static_cast<int>(WebInputEventResult::HandledApplication) < static_cast<int>(WebInputEventResult::HandledSystem), "WebInputEventResult not ordered");
    return static_cast<WebInputEventResult>(max(static_cast<int>(resultA), static_cast<int>(resultB)));
}

WebInputEventResult toWebInputEventResult(
    DispatchEventResult result)
{
    switch (result) {
    case DispatchEventResult::NotCanceled:
        return WebInputEventResult::NotHandled;
    case DispatchEventResult::CanceledByEventHandler:
        return WebInputEventResult::HandledApplication;
    case DispatchEventResult::CanceledByDefaultEventHandler:
        return WebInputEventResult::HandledSystem;
    case DispatchEventResult::CanceledBeforeDispatch:
        return WebInputEventResult::HandledSuppressed;
    default:
        NOTREACHED();
        return WebInputEventResult::HandledSystem;
    }
}

} // namespace EventHandlingUtil
} // namespace blink
