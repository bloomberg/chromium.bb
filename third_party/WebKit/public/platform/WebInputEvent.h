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

#ifndef WebInputEvent_h
#define WebInputEvent_h

#include "WebCommon.h"
#include "WebPointerProperties.h"
#include "WebRect.h"
#include "WebTouchPoint.h"

#include <string.h>

namespace blink {

// The classes defined in this file are intended to be used with
// WebWidget's handleInputEvent method.  These event types are cross-
// platform and correspond closely to WebCore's Platform*Event classes.
//
// WARNING! These classes must remain PODs (plain old data).  They are
// intended to be "serializable" by copying their raw bytes, so they must
// not contain any non-bit-copyable member variables!
//
// Furthermore, the class members need to be packed so they are aligned
// properly and don't have paddings/gaps, otherwise memory check tools
// like Valgrind will complain about uninitialized memory usage when
// transferring these classes over the wire.

#pragma pack(push, 4)

// WebInputEvent --------------------------------------------------------------

class WebInputEvent {
 public:
  // When we use an input method (or an input method editor), we receive
  // two events for a keypress. The former event is a keydown, which
  // provides a keycode, and the latter is a textinput, which provides
  // a character processed by an input method. (The mapping from a
  // keycode to a character code is not trivial for non-English
  // keyboards.)
  // To support input methods, Safari sends keydown events to WebKit for
  // filtering. WebKit sends filtered keydown events back to Safari,
  // which sends them to input methods.
  // Unfortunately, it is hard to apply this design to Chrome because of
  // our multiprocess architecture. An input method is running in a
  // browser process. On the other hand, WebKit is running in a renderer
  // process. So, this design results in increasing IPC messages.
  // To support input methods without increasing IPC messages, Chrome
  // handles keyboard events in a browser process and send asynchronous
  // input events (to be translated to DOM events) to a renderer
  // process.
  // This design is mostly the same as the one of Windows and Mac Carbon.
  // So, for what it's worth, our Linux and Mac front-ends emulate our
  // Windows front-end. To emulate our Windows front-end, we can share
  // our back-end code among Windows, Linux, and Mac.
  // TODO(hbono): Issue 18064: remove the KeyDown type since it isn't
  // used in Chrome any longer.

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.web
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: WebInputEventType
  enum Type {
    Undefined = -1,
    TypeFirst = Undefined,

    // WebMouseEvent
    MouseDown,
    MouseTypeFirst = MouseDown,
    MouseUp,
    MouseMove,
    MouseEnter,
    MouseLeave,
    ContextMenu,
    MouseTypeLast = ContextMenu,

    // WebMouseWheelEvent
    MouseWheel,

    // WebKeyboardEvent
    RawKeyDown,
    KeyboardTypeFirst = RawKeyDown,
    KeyDown,
    KeyUp,
    Char,
    KeyboardTypeLast = Char,

    // WebGestureEvent
    GestureScrollBegin,
    GestureTypeFirst = GestureScrollBegin,
    GestureScrollEnd,
    GestureScrollUpdate,
    GestureFlingStart,
    GestureFlingCancel,
    GestureShowPress,
    GestureTap,
    GestureTapUnconfirmed,
    GestureTapDown,
    GestureTapCancel,
    GestureDoubleTap,
    GestureTwoFingerTap,
    GestureLongPress,
    GestureLongTap,
    GesturePinchBegin,
    GesturePinchEnd,
    GesturePinchUpdate,
    GestureTypeLast = GesturePinchUpdate,

    // WebTouchEvent
    TouchStart,
    TouchTypeFirst = TouchStart,
    TouchMove,
    TouchEnd,
    TouchCancel,
    TouchScrollStarted,
    TouchTypeLast = TouchScrollStarted,

    TypeLast = TouchTypeLast
  };

  // The modifier constants cannot change their values since pepper
  // does a 1-1 mapping of its values; see
  // content/renderer/pepper/event_conversion.cc
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.web
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: WebInputEventModifier
  enum Modifiers {
    // modifiers for all events:
    ShiftKey = 1 << 0,
    ControlKey = 1 << 1,
    AltKey = 1 << 2,
    MetaKey = 1 << 3,

    // modifiers for keyboard events:
    IsKeyPad = 1 << 4,
    IsAutoRepeat = 1 << 5,

    // modifiers for mouse events:
    LeftButtonDown = 1 << 6,
    MiddleButtonDown = 1 << 7,
    RightButtonDown = 1 << 8,

    // Toggle modifers for all events.
    CapsLockOn = 1 << 9,
    NumLockOn = 1 << 10,

    IsLeft = 1 << 11,
    IsRight = 1 << 12,

    // Indicates that an event was generated on the touch screen while
    // touch accessibility is enabled, so the event should be handled
    // by accessibility code first before normal input event processing.
    IsTouchAccessibility = 1 << 13,

    IsComposing = 1 << 14,

    AltGrKey = 1 << 15,
    FnKey = 1 << 16,
    SymbolKey = 1 << 17,

    ScrollLockOn = 1 << 18,

    // Whether this is a compatibility event generated due to a
    // native touch event. Mouse events generated from touch
    // events will set this.
    IsCompatibilityEventForTouch = 1 << 19,

    // The set of non-stateful modifiers that specifically change the
    // interpretation of the key being pressed. For example; IsLeft,
    // IsRight, IsComposing don't change the meaning of the key
    // being pressed. NumLockOn, ScrollLockOn, CapsLockOn are stateful
    // and don't indicate explicit depressed state.
    KeyModifiers =
        SymbolKey | FnKey | AltGrKey | MetaKey | AltKey | ControlKey | ShiftKey,
    NoModifiers = 0,
  };

  // Indicates whether the browser needs to block on the ACK result for
  // this event, and if not, why (for metrics/diagnostics purposes).
  // These values are direct mappings of the values in PlatformEvent
  // so the values can be cast between the enumerations. static_asserts
  // checking this are in web/WebInputEventConversion.cpp.
  enum DispatchType {
    // Event can be canceled.
    Blocking,
    // Event can not be canceled.
    EventNonBlocking,
    // All listeners are passive; not cancelable.
    ListenersNonBlockingPassive,
    // This value represents a state which would have normally blocking
    // but was forced to be non-blocking during fling; not cancelable.
    ListenersForcedNonBlockingDueToFling,
    // This value represents a state which would have normally blocking but
    // was forced to be non-blocking due to the main thread being
    // unresponsive.
    ListenersForcedNonBlockingDueToMainThreadResponsiveness,
  };

  // The rail mode for a wheel event specifies the axis on which scrolling is
  // expected to stick. If this axis is set to Free, then scrolling is not
  // stuck to any axis.
  enum RailsMode {
    RailsModeFree = 0,
    RailsModeHorizontal = 1,
    RailsModeVertical = 2,
  };

  static const int InputModifiers = ShiftKey | ControlKey | AltKey | MetaKey;

  static constexpr double TimeStampForTesting = 123.0;

  // Returns true if the WebInputEvent |type| is a mouse event.
  static bool isMouseEventType(int type) {
    return MouseTypeFirst <= type && type <= MouseTypeLast;
  }

  // Returns true if the WebInputEvent |type| is a keyboard event.
  static bool isKeyboardEventType(int type) {
    return KeyboardTypeFirst <= type && type <= KeyboardTypeLast;
  }

  // Returns true if the WebInputEvent |type| is a touch event.
  static bool isTouchEventType(int type) {
    return TouchTypeFirst <= type && type <= TouchTypeLast;
  }

  // Returns true if the WebInputEvent is a gesture event.
  static bool isGestureEventType(int type) {
    return GestureTypeFirst <= type && type <= GestureTypeLast;
  }

  bool isSameEventClass(const WebInputEvent& other) const {
    if (isMouseEventType(m_type))
      return isMouseEventType(other.m_type);
    if (isGestureEventType(m_type))
      return isGestureEventType(other.m_type);
    if (isTouchEventType(m_type))
      return isTouchEventType(other.m_type);
    if (isKeyboardEventType(m_type))
      return isKeyboardEventType(other.m_type);
    return m_type == other.m_type;
  }

  static const char* GetName(WebInputEvent::Type type) {
#define CASE_TYPE(t)     \
  case WebInputEvent::t: \
    return #t
    switch (type) {
      CASE_TYPE(Undefined);
      CASE_TYPE(MouseDown);
      CASE_TYPE(MouseUp);
      CASE_TYPE(MouseMove);
      CASE_TYPE(MouseEnter);
      CASE_TYPE(MouseLeave);
      CASE_TYPE(ContextMenu);
      CASE_TYPE(MouseWheel);
      CASE_TYPE(RawKeyDown);
      CASE_TYPE(KeyDown);
      CASE_TYPE(KeyUp);
      CASE_TYPE(Char);
      CASE_TYPE(GestureScrollBegin);
      CASE_TYPE(GestureScrollEnd);
      CASE_TYPE(GestureScrollUpdate);
      CASE_TYPE(GestureFlingStart);
      CASE_TYPE(GestureFlingCancel);
      CASE_TYPE(GestureShowPress);
      CASE_TYPE(GestureTap);
      CASE_TYPE(GestureTapUnconfirmed);
      CASE_TYPE(GestureTapDown);
      CASE_TYPE(GestureTapCancel);
      CASE_TYPE(GestureDoubleTap);
      CASE_TYPE(GestureTwoFingerTap);
      CASE_TYPE(GestureLongPress);
      CASE_TYPE(GestureLongTap);
      CASE_TYPE(GesturePinchBegin);
      CASE_TYPE(GesturePinchEnd);
      CASE_TYPE(GesturePinchUpdate);
      CASE_TYPE(TouchStart);
      CASE_TYPE(TouchMove);
      CASE_TYPE(TouchEnd);
      CASE_TYPE(TouchCancel);
      CASE_TYPE(TouchScrollStarted);
      default:
        NOTREACHED();
        return "";
    }
#undef CASE_TYPE
  }

  float frameScale() const { return m_frameScale; }
  void setFrameScale(float scale) { m_frameScale = scale; }

  WebFloatPoint frameTranslate() const { return m_frameTranslate; }
  void setFrameTranslate(WebFloatPoint translate) {
    m_frameTranslate = translate;
  }

  Type type() const { return m_type; }
  void setType(Type typeParam) { m_type = typeParam; }

  int modifiers() const { return m_modifiers; }
  void setModifiers(int modifiersParam) { m_modifiers = modifiersParam; }

  double timeStampSeconds() const { return m_timeStampSeconds; }
  void setTimeStampSeconds(double seconds) { m_timeStampSeconds = seconds; }

  unsigned size() const { return m_size; }

 protected:
  // The root frame scale.
  float m_frameScale;

  // The root frame translation (applied post scale).
  WebFloatPoint m_frameTranslate;

  WebInputEvent(unsigned sizeParam,
                Type typeParam,
                int modifiersParam,
                double timeStampSecondsParam) {
    // TODO(dtapuska): Remove this memset when we remove the chrome IPC of this
    // struct.
    memset(this, 0, sizeParam);
    m_timeStampSeconds = timeStampSecondsParam;
    m_size = sizeParam;
    m_type = typeParam;
    m_modifiers = modifiersParam;
#if DCHECK_IS_ON()
    // If dcheck is on force failures if frame scale is not initialized
    // correctly by causing DIV0.
    m_frameScale = 0;
#else
    m_frameScale = 1;
#endif
  }

  WebInputEvent(unsigned sizeParam) {
    // TODO(dtapuska): Remove this memset when we remove the chrome IPC of this
    // struct.
    memset(this, 0, sizeParam);
    m_timeStampSeconds = 0.0;
    m_size = sizeParam;
    m_type = Undefined;
#if DCHECK_IS_ON()
    // If dcheck is on force failures if frame scale is not initialized
    // correctly by causing DIV0.
    m_frameScale = 0;
#else
    m_frameScale = 1;
#endif
  }

  double m_timeStampSeconds;  // Seconds since platform start with microsecond
                              // resolution.
  unsigned m_size;            // The size of this structure, for serialization.
  Type m_type;
  int m_modifiers;
};

#pragma pack(pop)

}  // namespace blink

#endif
