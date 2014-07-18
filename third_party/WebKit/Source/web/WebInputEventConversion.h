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

#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "public/web/WebInputEvent.h"

namespace blink {
class GestureEvent;
class KeyboardEvent;
class MouseEvent;
class RenderObject;
class ScrollView;
class TouchEvent;
class WheelEvent;
class Widget;
}

namespace blink {

class WebMouseEvent;
class WebMouseWheelEvent;
class WebKeyboardEvent;
class WebTouchEvent;
class WebGestureEvent;

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.

class PlatformMouseEventBuilder : public blink::PlatformMouseEvent {
public:
    PlatformMouseEventBuilder(blink::Widget*, const WebMouseEvent&);
};

class PlatformWheelEventBuilder : public blink::PlatformWheelEvent {
public:
    PlatformWheelEventBuilder(blink::Widget*, const WebMouseWheelEvent&);
};

class PlatformGestureEventBuilder : public blink::PlatformGestureEvent {
public:
    PlatformGestureEventBuilder(blink::Widget*, const WebGestureEvent&);
};

class PlatformKeyboardEventBuilder : public blink::PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(const WebKeyboardEvent&);
    void setKeyType(Type);
    bool isCharacterKey() const;
};

// Converts a WebTouchPoint to a blink::PlatformTouchPoint.
class PlatformTouchPointBuilder : public blink::PlatformTouchPoint {
public:
    PlatformTouchPointBuilder(blink::Widget*, const WebTouchPoint&);
};

// Converts a WebTouchEvent to a blink::PlatformTouchEvent.
class PlatformTouchEventBuilder : public blink::PlatformTouchEvent {
public:
    PlatformTouchEventBuilder(blink::Widget*, const WebTouchEvent&);
};

class WebMouseEventBuilder : public WebMouseEvent {
public:
    // Converts a blink::MouseEvent to a corresponding WebMouseEvent.
    // NOTE: This is only implemented for mousemove, mouseover, mouseout,
    // mousedown and mouseup. If the event mapping fails, the event type will
    // be set to Undefined.
    WebMouseEventBuilder(const blink::Widget*, const blink::RenderObject*, const blink::MouseEvent&);
    WebMouseEventBuilder(const blink::Widget*, const blink::RenderObject*, const blink::TouchEvent&);

    // Converts a blink::PlatformMouseEvent to a corresponding WebMouseEvent.
    // NOTE: This is only implemented for mousepressed, mousereleased, and
    // mousemoved. If the event mapping fails, the event type will be set to
    // Undefined.
    WebMouseEventBuilder(const blink::Widget*, const blink::PlatformMouseEvent&);
};

// Converts a blink::WheelEvent to a corresponding WebMouseWheelEvent.
// If the event mapping fails, the event type will be set to Undefined.
class WebMouseWheelEventBuilder : public WebMouseWheelEvent {
public:
    WebMouseWheelEventBuilder(const blink::Widget*, const blink::RenderObject*, const blink::WheelEvent&);
};

// Converts a blink::KeyboardEvent or blink::PlatformKeyboardEvent to a
// corresponding WebKeyboardEvent.
// NOTE: For blink::KeyboardEvent, this is only implemented for keydown,
// keyup, and keypress. If the event mapping fails, the event type will be set
// to Undefined.
class WebKeyboardEventBuilder : public WebKeyboardEvent {
public:
    WebKeyboardEventBuilder(const blink::KeyboardEvent&);
    WebKeyboardEventBuilder(const blink::PlatformKeyboardEvent&);
};

// Converts a blink::TouchEvent to a corresponding WebTouchEvent.
// NOTE: WebTouchEvents have a cap on the number of WebTouchPoints. Any points
// exceeding that cap will be dropped.
class WebTouchEventBuilder : public WebTouchEvent {
public:
    WebTouchEventBuilder(const blink::Widget*, const blink::RenderObject*, const blink::TouchEvent&);
};

// Converts blink::GestureEvent to a corresponding WebGestureEvent.
// NOTE: If event mapping fails, the type will be set to Undefined.
class WebGestureEventBuilder : public WebGestureEvent {
public:
    WebGestureEventBuilder(const blink::Widget*, const blink::RenderObject*, const blink::GestureEvent&);
};

} // namespace blink

#endif
