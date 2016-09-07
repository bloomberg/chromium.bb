// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventHandlingUtil_h
#define EventHandlingUtil_h

#include "core/frame/LocalFrame.h"
#include "core/layout/HitTestResult.h"
#include "platform/geometry/LayoutPoint.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

namespace EventHandlingUtil {

HitTestResult hitTestResultInFrame(LocalFrame*, const LayoutPoint&,
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly
        | HitTestRequest::Active);

WebInputEventResult mergeEventResult(
    WebInputEventResult resultA, WebInputEventResult resultB);
WebInputEventResult toWebInputEventResult(DispatchEventResult);

} // namespace EventHandlingUtil

} // namespace blink

#endif // EventHandlingUtil_h
