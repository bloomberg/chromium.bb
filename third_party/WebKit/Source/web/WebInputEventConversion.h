/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebInputEventConversion_h
#define WebInputEventConversion_h

#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebKeyboardEvent.h"
#include "public/platform/WebMouseWheelEvent.h"
#include "public/platform/WebTouchEvent.h"
#include "web/WebExport.h"
#include "wtf/Compiler.h"
#include <vector>

namespace blink {

class FrameViewBase;
class KeyboardEvent;
class MouseEvent;
class LayoutItem;
class TouchEvent;
class WebGestureEvent;
class WebKeyboardEvent;

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.


class WEB_EXPORT WebMouseEventBuilder
    : NON_EXPORTED_BASE(public WebMouseEvent) {
 public:
  // Converts a MouseEvent to a corresponding WebMouseEvent.
  // NOTE: This is only implemented for mousemove, mouseover, mouseout,
  // mousedown and mouseup. If the event mapping fails, the event type will
  // be set to Undefined.
  WebMouseEventBuilder(const FrameViewBase*,
                       const LayoutItem,
                       const MouseEvent&);
  WebMouseEventBuilder(const FrameViewBase*,
                       const LayoutItem,
                       const TouchEvent&);
};

// Converts a KeyboardEvent to a corresponding WebKeyboardEvent.
// NOTE: For KeyboardEvent, this is only implemented for keydown,
// keyup, and keypress. If the event mapping fails, the event type will be set
// to Undefined.
class WEB_EXPORT WebKeyboardEventBuilder
    : NON_EXPORTED_BASE(public WebKeyboardEvent) {
 public:
  WebKeyboardEventBuilder(const KeyboardEvent&);
};

// Converts a TouchEvent to a corresponding WebTouchEvent.
// NOTE: WebTouchEvents have a cap on the number of WebTouchPoints. Any points
// exceeding that cap will be dropped.
class WEB_EXPORT WebTouchEventBuilder
    : NON_EXPORTED_BASE(public WebTouchEvent) {
 public:
  WebTouchEventBuilder(const LayoutItem, const TouchEvent&);
};

// Return a new transformed WebGestureEvent by applying the Widget's scale
// and translation.
WEB_EXPORT WebGestureEvent TransformWebGestureEvent(FrameViewBase*,
                                                    const WebGestureEvent&);
WEB_EXPORT WebMouseEvent TransformWebMouseEvent(FrameViewBase*,
                                                const WebMouseEvent&);

WEB_EXPORT WebMouseWheelEvent
TransformWebMouseWheelEvent(FrameViewBase*, const WebMouseWheelEvent&);

WEB_EXPORT WebTouchEvent TransformWebTouchEvent(FrameViewBase*,
                                                const WebTouchEvent&);

Vector<WebMouseEvent> WEB_EXPORT
TransformWebMouseEventVector(FrameViewBase*,
                             const std::vector<const WebInputEvent*>&);
Vector<WebTouchEvent> WEB_EXPORT
TransformWebTouchEventVector(FrameViewBase*,
                             const std::vector<const WebInputEvent*>&);

}  // namespace blink

#endif
