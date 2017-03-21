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

#include "web/WebInputEventConversion.h"

#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/api/LayoutItem.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/FrameViewBase.h"
#include "platform/KeyboardCodes.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {
float frameScale(const FrameViewBase* frameViewBase) {
  float scale = 1;
  if (frameViewBase) {
    FrameView* rootView = toFrameView(frameViewBase->root());
    if (rootView)
      scale = rootView->inputEventsScaleFactor();
  }
  return scale;
}

FloatPoint frameTranslation(const FrameViewBase* frameViewBase) {
  float scale = 1;
  FloatSize offset;
  IntPoint visualViewport;
  FloatSize overscrollOffset;
  if (frameViewBase) {
    FrameView* rootView = toFrameView(frameViewBase->root());
    if (rootView) {
      scale = rootView->inputEventsScaleFactor();
      offset = FloatSize(rootView->inputEventsOffsetForEmulation());
      visualViewport = flooredIntPoint(
          rootView->page()->visualViewport().visibleRect().location());
      overscrollOffset = rootView->page()->chromeClient().elasticOverscroll();
    }
  }
  return FloatPoint(
      -offset.width() / scale + visualViewport.x() + overscrollOffset.width(),
      -offset.height() / scale + visualViewport.y() +
          overscrollOffset.height());
}

FloatPoint convertAbsoluteLocationForLayoutObjectFloat(
    const DoublePoint& location,
    const LayoutItem layoutItem) {
  return layoutItem.absoluteToLocal(FloatPoint(location), UseTransforms);
}

IntPoint convertAbsoluteLocationForLayoutObjectInt(
    const DoublePoint& location,
    const LayoutItem layoutItem) {
  return roundedIntPoint(
      convertAbsoluteLocationForLayoutObjectFloat(location, layoutItem));
}

// FIXME: Change |FrameViewBase| to const FrameViewBase& after RemoteFrames get
// RemoteFrameViews.
void updateWebMouseEventFromCoreMouseEvent(const MouseEvent& event,
                                           const FrameViewBase* frameViewBase,
                                           const LayoutItem layoutItem,
                                           WebMouseEvent& webEvent) {
  webEvent.setTimeStampSeconds(event.platformTimeStamp().InSeconds());
  webEvent.setModifiers(event.modifiers());

  FrameView* view = frameViewBase ? toFrameView(frameViewBase->parent()) : 0;
  // TODO(bokan): If view == nullptr, pointInRootFrame will really be
  // pointInRootContent.
  IntPoint pointInRootFrame(event.absoluteLocation().x(),
                            event.absoluteLocation().y());
  if (view)
    pointInRootFrame = view->contentsToRootFrame(pointInRootFrame);
  webEvent.globalX = event.screenX();
  webEvent.globalY = event.screenY();
  IntPoint localPoint = convertAbsoluteLocationForLayoutObjectInt(
      event.absoluteLocation(), layoutItem);
  webEvent.x = localPoint.x();
  webEvent.y = localPoint.y();
}

unsigned toWebInputEventModifierFrom(WebMouseEvent::Button button) {
  if (button == WebMouseEvent::Button::NoButton)
    return 0;

  unsigned webMouseButtonToPlatformModifier[] = {
      WebInputEvent::LeftButtonDown, WebInputEvent::MiddleButtonDown,
      WebInputEvent::RightButtonDown};

  return webMouseButtonToPlatformModifier[static_cast<int>(button)];
}

}  // namespace

WebMouseEvent TransformWebMouseEvent(FrameViewBase* frameViewBase,
                                     const WebMouseEvent& event) {
  WebMouseEvent result = event;

  // TODO(dtapuska): Remove this translation. In the past blink has
  // converted leaves into moves and not known about leaves. It should
  // be educated about them. crbug.com/686196
  if (event.type() == WebInputEvent::MouseEnter ||
      event.type() == WebInputEvent::MouseLeave) {
    result.setType(WebInputEvent::MouseMove);
  }

  // TODO(dtapuska): Perhaps the event should be constructed correctly?
  // crbug.com/686200
  if (event.type() == WebInputEvent::MouseUp) {
    result.setModifiers(event.modifiers() &
                        ~toWebInputEventModifierFrom(event.button));
  }
  result.setFrameScale(frameScale(frameViewBase));
  result.setFrameTranslate(frameTranslation(frameViewBase));
  return result;
}

WebMouseWheelEvent TransformWebMouseWheelEvent(
    FrameViewBase* frameViewBase,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent result = event;
  result.setFrameScale(frameScale(frameViewBase));
  result.setFrameTranslate(frameTranslation(frameViewBase));
  return result;
}

WebGestureEvent TransformWebGestureEvent(FrameViewBase* frameViewBase,
                                         const WebGestureEvent& event) {
  WebGestureEvent result = event;
  result.setFrameScale(frameScale(frameViewBase));
  result.setFrameTranslate(frameTranslation(frameViewBase));
  return result;
}

WebTouchEvent TransformWebTouchEvent(float frameScale,
                                     FloatPoint frameTranslate,
                                     const WebTouchEvent& event) {
  // frameScale is default initialized in debug builds to be 0.
  DCHECK_EQ(0, event.frameScale());
  DCHECK_EQ(0, event.frameTranslate().x);
  DCHECK_EQ(0, event.frameTranslate().y);
  WebTouchEvent result = event;
  result.setFrameScale(frameScale);
  result.setFrameTranslate(frameTranslate);
  return result;
}

WebTouchEvent TransformWebTouchEvent(FrameViewBase* frameViewBase,
                                     const WebTouchEvent& event) {
  return TransformWebTouchEvent(frameScale(frameViewBase),
                                frameTranslation(frameViewBase), event);
}

WebMouseEventBuilder::WebMouseEventBuilder(const FrameViewBase* frameViewBase,
                                           const LayoutItem layoutItem,
                                           const MouseEvent& event) {
  if (event.nativeEvent()) {
    *static_cast<WebMouseEvent*>(this) =
        event.nativeEvent()->flattenTransform();
    WebFloatPoint absoluteRootFrameLocation = positionInRootFrame();
    IntPoint localPoint = roundedIntPoint(
        layoutItem.absoluteToLocal(absoluteRootFrameLocation, UseTransforms));
    x = localPoint.x();
    y = localPoint.y();
    return;
  }

  // Code below here can be removed once OOPIF ships.
  // OOPIF will prevent synthetic events being dispatched into
  // other frames; but for now we allow the fallback to generate
  // WebMouseEvents from synthetic events.
  if (event.type() == EventTypeNames::mousemove)
    m_type = WebInputEvent::MouseMove;
  else if (event.type() == EventTypeNames::mouseout)
    m_type = WebInputEvent::MouseLeave;
  else if (event.type() == EventTypeNames::mouseover)
    m_type = WebInputEvent::MouseEnter;
  else if (event.type() == EventTypeNames::mousedown)
    m_type = WebInputEvent::MouseDown;
  else if (event.type() == EventTypeNames::mouseup)
    m_type = WebInputEvent::MouseUp;
  else if (event.type() == EventTypeNames::contextmenu)
    m_type = WebInputEvent::ContextMenu;
  else
    return;  // Skip all other mouse events.

  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  m_modifiers = event.modifiers();
  updateWebMouseEventFromCoreMouseEvent(event, frameViewBase, layoutItem,
                                        *this);

  switch (event.button()) {
    case short(WebPointerProperties::Button::Left):
      button = WebMouseEvent::Button::Left;
      break;
    case short(WebPointerProperties::Button::Middle):
      button = WebMouseEvent::Button::Middle;
      break;
    case short(WebPointerProperties::Button::Right):
      button = WebMouseEvent::Button::Right;
      break;
  }
  if (event.buttonDown()) {
    switch (event.button()) {
      case short(WebPointerProperties::Button::Left):
        m_modifiers |= WebInputEvent::LeftButtonDown;
        break;
      case short(WebPointerProperties::Button::Middle):
        m_modifiers |= WebInputEvent::MiddleButtonDown;
        break;
      case short(WebPointerProperties::Button::Right):
        m_modifiers |= WebInputEvent::RightButtonDown;
        break;
    }
  } else {
    button = WebMouseEvent::Button::NoButton;
  }
  movementX = event.movementX();
  movementY = event.movementY();
  clickCount = event.detail();

  pointerType = WebPointerProperties::PointerType::Mouse;
}

// Generate a synthetic WebMouseEvent given a TouchEvent (eg. for emulating a
// mouse with touch input for plugins that don't support touch input).
WebMouseEventBuilder::WebMouseEventBuilder(const FrameViewBase* frameViewBase,
                                           const LayoutItem layoutItem,
                                           const TouchEvent& event) {
  if (!event.touches())
    return;
  if (event.touches()->length() != 1) {
    if (event.touches()->length() || event.type() != EventTypeNames::touchend ||
        !event.changedTouches() || event.changedTouches()->length() != 1)
      return;
  }

  const Touch* touch = event.touches()->length() == 1
                           ? event.touches()->item(0)
                           : event.changedTouches()->item(0);
  if (touch->identifier())
    return;

  if (event.type() == EventTypeNames::touchstart)
    m_type = MouseDown;
  else if (event.type() == EventTypeNames::touchmove)
    m_type = MouseMove;
  else if (event.type() == EventTypeNames::touchend)
    m_type = MouseUp;
  else
    return;

  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  m_modifiers = event.modifiers();
  m_frameScale = 1;
  m_frameTranslate = WebFloatPoint();

  // The mouse event co-ordinates should be generated from the co-ordinates of
  // the touch point.
  FrameView* view = toFrameView(frameViewBase->parent());
  // FIXME: if view == nullptr, pointInRootFrame will really be
  // pointInRootContent.
  IntPoint pointInRootFrame = roundedIntPoint(touch->absoluteLocation());
  if (view)
    pointInRootFrame = view->contentsToRootFrame(pointInRootFrame);
  IntPoint screenPoint = roundedIntPoint(touch->screenLocation());
  globalX = screenPoint.x();
  globalY = screenPoint.y();

  button = WebMouseEvent::Button::Left;
  m_modifiers |= WebInputEvent::LeftButtonDown;
  clickCount = (m_type == MouseDown || m_type == MouseUp);

  IntPoint localPoint = convertAbsoluteLocationForLayoutObjectInt(
      DoublePoint(touch->absoluteLocation()), layoutItem);
  x = localPoint.x();
  y = localPoint.y();

  pointerType = WebPointerProperties::PointerType::Touch;
}

WebKeyboardEventBuilder::WebKeyboardEventBuilder(const KeyboardEvent& event) {
  if (const WebKeyboardEvent* webEvent = event.keyEvent()) {
    *static_cast<WebKeyboardEvent*>(this) = *webEvent;

    // TODO(dtapuska): DOM KeyboardEvents converted back to WebInputEvents
    // drop the Raw behaviour. Figure out if this is actually really needed.
    if (m_type == RawKeyDown)
      m_type = KeyDown;
    return;
  }

  if (event.type() == EventTypeNames::keydown)
    m_type = KeyDown;
  else if (event.type() == EventTypeNames::keyup)
    m_type = WebInputEvent::KeyUp;
  else if (event.type() == EventTypeNames::keypress)
    m_type = WebInputEvent::Char;
  else
    return;  // Skip all other keyboard events.

  m_modifiers = event.modifiers();
  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  windowsKeyCode = event.keyCode();
}

Vector<WebMouseEvent> TransformWebMouseEventVector(
    FrameViewBase* frameViewBase,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  Vector<WebMouseEvent> result;
  for (const auto& event : coalescedEvents) {
    DCHECK(WebInputEvent::isMouseEventType(event->type()));
    result.push_back(TransformWebMouseEvent(
        frameViewBase, static_cast<const WebMouseEvent&>(*event)));
  }
  return result;
}

Vector<WebTouchEvent> TransformWebTouchEventVector(
    FrameViewBase* frameViewBase,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  float scale = frameScale(frameViewBase);
  FloatPoint translation = frameTranslation(frameViewBase);
  Vector<WebTouchEvent> result;
  for (const auto& event : coalescedEvents) {
    DCHECK(WebInputEvent::isTouchEventType(event->type()));
    result.push_back(TransformWebTouchEvent(
        scale, translation, static_cast<const WebTouchEvent&>(*event)));
  }
  return result;
}

}  // namespace blink
