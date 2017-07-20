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

#include "core/events/WebInputEventConversion.h"

#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/api/LayoutItem.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/KeyboardCodes.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {
float FrameScale(const LocalFrameView* frame_view) {
  float scale = 1;
  if (frame_view) {
    LocalFrameView* root_view = frame_view->GetFrame().LocalFrameRoot().View();
    if (root_view)
      scale = root_view->InputEventsScaleFactor();
  }
  return scale;
}

FloatPoint FrameTranslation(const LocalFrameView* frame_view) {
  float scale = 1;
  IntPoint visual_viewport;
  FloatSize overscroll_offset;
  if (frame_view) {
    LocalFrameView* root_view = frame_view->GetFrame().LocalFrameRoot().View();
    if (root_view) {
      scale = root_view->InputEventsScaleFactor();
      visual_viewport = FlooredIntPoint(
          root_view->GetPage()->GetVisualViewport().VisibleRect().Location());
      overscroll_offset =
          root_view->GetPage()->GetChromeClient().ElasticOverscroll();
    }
  }
  return FloatPoint(visual_viewport.X() + overscroll_offset.Width(),
                    visual_viewport.Y() + overscroll_offset.Height());
}

FloatPoint ConvertAbsoluteLocationForLayoutObjectFloat(
    const DoublePoint& location,
    const LayoutItem layout_item) {
  return layout_item.AbsoluteToLocal(FloatPoint(location), kUseTransforms);
}

IntPoint ConvertAbsoluteLocationForLayoutObjectInt(
    const DoublePoint& location,
    const LayoutItem layout_item) {
  return RoundedIntPoint(
      ConvertAbsoluteLocationForLayoutObjectFloat(location, layout_item));
}

// FIXME: Change |LocalFrameView| to const FrameView& after RemoteFrames get
// RemoteFrameViews.
void UpdateWebMouseEventFromCoreMouseEvent(const MouseEvent& event,
                                           const LocalFrameView* plugin_parent,
                                           const LayoutItem layout_item,
                                           WebMouseEvent& web_event) {
  web_event.SetTimeStampSeconds(event.PlatformTimeStamp().InSeconds());
  web_event.SetModifiers(event.GetModifiers());

  // TODO(bokan): If plugin_parent == nullptr, pointInRootFrame will really be
  // pointInRootContent.
  IntPoint point_in_root_frame(event.AbsoluteLocation().X(),
                               event.AbsoluteLocation().Y());
  if (plugin_parent) {
    point_in_root_frame =
        plugin_parent->ContentsToRootFrame(point_in_root_frame);
  }
  web_event.SetPositionInScreen(event.screenX(), event.screenY());
  IntPoint local_point = ConvertAbsoluteLocationForLayoutObjectInt(
      event.AbsoluteLocation(), layout_item);
  web_event.SetPositionInWidget(local_point.X(), local_point.Y());
}

unsigned ToWebInputEventModifierFrom(WebMouseEvent::Button button) {
  if (button == WebMouseEvent::Button::kNoButton)
    return 0;

  unsigned web_mouse_button_to_platform_modifier[] = {
      WebInputEvent::kLeftButtonDown, WebInputEvent::kMiddleButtonDown,
      WebInputEvent::kRightButtonDown, WebInputEvent::kBackButtonDown,
      WebInputEvent::kForwardButtonDown};

  return web_mouse_button_to_platform_modifier[static_cast<int>(button)];
}

}  // namespace

WebMouseEvent TransformWebMouseEvent(LocalFrameView* frame_view,
                                     const WebMouseEvent& event) {
  WebMouseEvent result = event;

  // TODO(dtapuska): Perhaps the event should be constructed correctly?
  // crbug.com/686200
  if (event.GetType() == WebInputEvent::kMouseUp) {
    result.SetModifiers(event.GetModifiers() &
                        ~ToWebInputEventModifierFrom(event.button));
  }
  result.SetFrameScale(FrameScale(frame_view));
  result.SetFrameTranslate(FrameTranslation(frame_view));
  return result;
}

WebMouseWheelEvent TransformWebMouseWheelEvent(
    LocalFrameView* frame_view,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent result = event;
  result.SetFrameScale(FrameScale(frame_view));
  result.SetFrameTranslate(FrameTranslation(frame_view));
  return result;
}

WebGestureEvent TransformWebGestureEvent(LocalFrameView* frame_view,
                                         const WebGestureEvent& event) {
  WebGestureEvent result = event;
  result.SetFrameScale(FrameScale(frame_view));
  result.SetFrameTranslate(FrameTranslation(frame_view));
  return result;
}

WebTouchEvent TransformWebTouchEvent(float frame_scale,
                                     FloatPoint frame_translate,
                                     const WebTouchEvent& event) {
  // frameScale is default initialized in debug builds to be 0.
  DCHECK_EQ(0, event.FrameScale());
  DCHECK_EQ(0, event.FrameTranslate().x);
  DCHECK_EQ(0, event.FrameTranslate().y);
  WebTouchEvent result = event;
  result.SetFrameScale(frame_scale);
  result.SetFrameTranslate(frame_translate);
  return result;
}

WebTouchEvent TransformWebTouchEvent(LocalFrameView* frame_view,
                                     const WebTouchEvent& event) {
  return TransformWebTouchEvent(FrameScale(frame_view),
                                FrameTranslation(frame_view), event);
}

WebMouseEventBuilder::WebMouseEventBuilder(const LocalFrameView* plugin_parent,
                                           const LayoutItem layout_item,
                                           const MouseEvent& event) {
  if (event.NativeEvent()) {
    *static_cast<WebMouseEvent*>(this) =
        event.NativeEvent()->FlattenTransform();
    WebFloatPoint absolute_location = PositionInRootFrame();

    // TODO(dtapuska): |plugin_parent| should never be null. Remove this
    // conditional code.
    // Translate the root frame position to content coordinates.
    if (plugin_parent) {
      absolute_location = plugin_parent->RootFrameToContents(absolute_location);
    }

    IntPoint local_point = RoundedIntPoint(
        layout_item.AbsoluteToLocal(absolute_location, kUseTransforms));
    SetPositionInWidget(local_point.X(), local_point.Y());
    return;
  }

  // Code below here can be removed once OOPIF ships.
  // OOPIF will prevent synthetic events being dispatched into
  // other frames; but for now we allow the fallback to generate
  // WebMouseEvents from synthetic events.
  if (event.type() == EventTypeNames::mousemove)
    type_ = WebInputEvent::kMouseMove;
  else if (event.type() == EventTypeNames::mouseout)
    type_ = WebInputEvent::kMouseLeave;
  else if (event.type() == EventTypeNames::mouseover)
    type_ = WebInputEvent::kMouseEnter;
  else if (event.type() == EventTypeNames::mousedown)
    type_ = WebInputEvent::kMouseDown;
  else if (event.type() == EventTypeNames::mouseup)
    type_ = WebInputEvent::kMouseUp;
  else if (event.type() == EventTypeNames::contextmenu)
    type_ = WebInputEvent::kContextMenu;
  else
    return;  // Skip all other mouse events.

  time_stamp_seconds_ = event.PlatformTimeStamp().InSeconds();
  modifiers_ = event.GetModifiers();
  UpdateWebMouseEventFromCoreMouseEvent(event, plugin_parent, layout_item,
                                        *this);

  switch (event.button()) {
    case short(WebPointerProperties::Button::kLeft):
      button = WebMouseEvent::Button::kLeft;
      break;
    case short(WebPointerProperties::Button::kMiddle):
      button = WebMouseEvent::Button::kMiddle;
      break;
    case short(WebPointerProperties::Button::kRight):
      button = WebMouseEvent::Button::kRight;
      break;
    case short(WebPointerProperties::Button::kBack):
      button = WebMouseEvent::Button::kBack;
      break;
    case short(WebPointerProperties::Button::kForward):
      button = WebMouseEvent::Button::kForward;
      break;
  }
  if (event.ButtonDown()) {
    switch (event.button()) {
      case short(WebPointerProperties::Button::kLeft):
        modifiers_ |= WebInputEvent::kLeftButtonDown;
        break;
      case short(WebPointerProperties::Button::kMiddle):
        modifiers_ |= WebInputEvent::kMiddleButtonDown;
        break;
      case short(WebPointerProperties::Button::kRight):
        modifiers_ |= WebInputEvent::kRightButtonDown;
        break;
      case short(WebPointerProperties::Button::kBack):
        modifiers_ |= WebInputEvent::kBackButtonDown;
        break;
      case short(WebPointerProperties::Button::kForward):
        modifiers_ |= WebInputEvent::kForwardButtonDown;
        break;
    }
  } else {
    button = WebMouseEvent::Button::kNoButton;
  }
  movement_x = event.movementX();
  movement_y = event.movementY();
  click_count = event.detail();

  pointer_type = WebPointerProperties::PointerType::kMouse;
}

// Generate a synthetic WebMouseEvent given a TouchEvent (eg. for emulating a
// mouse with touch input for plugins that don't support touch input).
WebMouseEventBuilder::WebMouseEventBuilder(const LocalFrameView* plugin_parent,
                                           const LayoutItem layout_item,
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
    type_ = kMouseDown;
  else if (event.type() == EventTypeNames::touchmove)
    type_ = kMouseMove;
  else if (event.type() == EventTypeNames::touchend)
    type_ = kMouseUp;
  else
    return;

  time_stamp_seconds_ = event.PlatformTimeStamp().InSeconds();
  modifiers_ = event.GetModifiers();
  frame_scale_ = 1;
  frame_translate_ = WebFloatPoint();

  // The mouse event co-ordinates should be generated from the co-ordinates of
  // the touch point.
  // FIXME: if plugin_parent == nullptr, pointInRootFrame will really be
  // pointInRootContent.
  IntPoint point_in_root_frame = RoundedIntPoint(touch->AbsoluteLocation());
  if (plugin_parent) {
    point_in_root_frame =
        plugin_parent->ContentsToRootFrame(point_in_root_frame);
  }
  IntPoint screen_point = RoundedIntPoint(touch->ScreenLocation());
  SetPositionInScreen(screen_point.X(), screen_point.Y());

  button = WebMouseEvent::Button::kLeft;
  modifiers_ |= WebInputEvent::kLeftButtonDown;
  click_count = (type_ == kMouseDown || type_ == kMouseUp);

  IntPoint local_point = ConvertAbsoluteLocationForLayoutObjectInt(
      DoublePoint(touch->AbsoluteLocation()), layout_item);
  SetPositionInWidget(local_point.X(), local_point.Y());

  pointer_type = WebPointerProperties::PointerType::kTouch;
}

WebKeyboardEventBuilder::WebKeyboardEventBuilder(const KeyboardEvent& event) {
  if (const WebKeyboardEvent* web_event = event.KeyEvent()) {
    *static_cast<WebKeyboardEvent*>(this) = *web_event;

    // TODO(dtapuska): DOM KeyboardEvents converted back to WebInputEvents
    // drop the Raw behaviour. Figure out if this is actually really needed.
    if (type_ == kRawKeyDown)
      type_ = kKeyDown;
    return;
  }

  if (event.type() == EventTypeNames::keydown)
    type_ = kKeyDown;
  else if (event.type() == EventTypeNames::keyup)
    type_ = WebInputEvent::kKeyUp;
  else if (event.type() == EventTypeNames::keypress)
    type_ = WebInputEvent::kChar;
  else
    return;  // Skip all other keyboard events.

  modifiers_ = event.GetModifiers();
  time_stamp_seconds_ = event.PlatformTimeStamp().InSeconds();
  windows_key_code = event.keyCode();
}

Vector<WebMouseEvent> TransformWebMouseEventVector(
    LocalFrameView* frame_view,
    const std::vector<const WebInputEvent*>& coalesced_events) {
  Vector<WebMouseEvent> result;
  for (const auto& event : coalesced_events) {
    DCHECK(WebInputEvent::IsMouseEventType(event->GetType()));
    result.push_back(TransformWebMouseEvent(
        frame_view, static_cast<const WebMouseEvent&>(*event)));
  }
  return result;
}

Vector<WebTouchEvent> TransformWebTouchEventVector(
    LocalFrameView* frame_view,
    const std::vector<const WebInputEvent*>& coalesced_events) {
  float scale = FrameScale(frame_view);
  FloatPoint translation = FrameTranslation(frame_view);
  Vector<WebTouchEvent> result;
  for (const auto& event : coalesced_events) {
    DCHECK(WebInputEvent::IsTouchEventType(event->GetType()));
    result.push_back(TransformWebTouchEvent(
        scale, translation, static_cast<const WebTouchEvent&>(*event)));
  }
  return result;
}

}  // namespace blink
