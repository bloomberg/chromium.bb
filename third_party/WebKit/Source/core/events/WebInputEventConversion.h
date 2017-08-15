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

#include <vector>
#include "core/CoreExport.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Compiler.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebKeyboardEvent.h"
#include "public/platform/WebMouseWheelEvent.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

class KeyboardEvent;
class LayoutItem;
class LocalFrameView;
class MouseEvent;
class TouchEvent;
class WebGestureEvent;
class WebKeyboardEvent;

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.

class CORE_EXPORT WebMouseEventBuilder : public WebMouseEvent {
 public:
  // Converts a MouseEvent to a corresponding WebMouseEvent.
  // NOTE: This is only implemented for mousemove, mouseover, mouseout,
  // mousedown and mouseup. If the event mapping fails, the event type will
  // be set to Undefined.
  WebMouseEventBuilder(const LocalFrameView*,
                       const LayoutItem,
                       const MouseEvent&);
  WebMouseEventBuilder(const LocalFrameView*,
                       const LayoutItem,
                       const TouchEvent&);
};

// Converts a KeyboardEvent to a corresponding WebKeyboardEvent.
// NOTE: For KeyboardEvent, this is only implemented for keydown,
// keyup, and keypress. If the event mapping fails, the event type will be set
// to Undefined.
class CORE_EXPORT WebKeyboardEventBuilder : public WebKeyboardEvent {
 public:
  WebKeyboardEventBuilder(const KeyboardEvent&);
};

// Converts a TouchEvent to a corresponding WebTouchEvent.
// NOTE: WebTouchEvents have a cap on the number of WebTouchPoints. Any points
// exceeding that cap will be dropped.
class CORE_EXPORT WebTouchEventBuilder : public WebTouchEvent {
 public:
  WebTouchEventBuilder(const LayoutItem, const TouchEvent&);
};

// Return a new transformed WebGestureEvent by applying the Widget's scale
// and translation.
CORE_EXPORT WebGestureEvent TransformWebGestureEvent(LocalFrameView*,
                                                     const WebGestureEvent&);
CORE_EXPORT WebMouseEvent TransformWebMouseEvent(LocalFrameView*,
                                                 const WebMouseEvent&);

CORE_EXPORT WebMouseWheelEvent
TransformWebMouseWheelEvent(LocalFrameView*, const WebMouseWheelEvent&);

CORE_EXPORT WebTouchEvent TransformWebTouchEvent(LocalFrameView*,
                                                 const WebTouchEvent&);

Vector<WebMouseEvent> CORE_EXPORT
TransformWebMouseEventVector(LocalFrameView*,
                             const std::vector<const WebInputEvent*>&);
Vector<WebTouchEvent> CORE_EXPORT
TransformWebTouchEventVector(LocalFrameView*,
                             const std::vector<const WebInputEvent*>&);

}  // namespace blink

#endif
