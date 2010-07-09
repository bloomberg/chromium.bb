// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_event_conversion.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

namespace {
// Anonymous namespace for functions converting WebInputEvent to PP_Event and
// back.
PP_Event_Type ConvertEventTypes(WebInputEvent::Type wetype) {
  switch (wetype) {
    case WebInputEvent::MouseDown:
      return PP_EVENT_TYPE_MOUSEDOWN;
    case WebInputEvent::MouseUp:
      return PP_EVENT_TYPE_MOUSEUP;
    case WebInputEvent::MouseMove:
      return PP_EVENT_TYPE_MOUSEMOVE;
    case WebInputEvent::MouseEnter:
      return PP_EVENT_TYPE_MOUSEENTER;
    case WebInputEvent::MouseLeave:
      return PP_EVENT_TYPE_MOUSELEAVE;
    case WebInputEvent::MouseWheel:
      return PP_EVENT_TYPE_MOUSEWHEEL;
    case WebInputEvent::RawKeyDown:
      return PP_EVENT_TYPE_RAWKEYDOWN;
    case WebInputEvent::KeyDown:
      return PP_EVENT_TYPE_KEYDOWN;
    case WebInputEvent::KeyUp:
      return PP_EVENT_TYPE_KEYUP;
    case WebInputEvent::Char:
      return PP_EVENT_TYPE_CHAR;
    case WebInputEvent::Undefined:
    default:
      return PP_EVENT_TYPE_UNDEFINED;
  }
}

void BuildKeyEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKeyboardEvent*>(event);
  pp_event->u.key.modifier = key_event->modifiers;
  pp_event->u.key.normalizedKeyCode = key_event->windowsKeyCode;
}

void BuildCharEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKeyboardEvent*>(event);
  pp_event->u.character.modifier = key_event->modifiers;
  // For consistency, check that the sizes of the texts agree.
  DCHECK(sizeof(pp_event->u.character.text) == sizeof(key_event->text));
  DCHECK(sizeof(pp_event->u.character.unmodifiedText) ==
         sizeof(key_event->unmodifiedText));
  for (size_t i = 0; i < WebKeyboardEvent::textLengthCap; ++i) {
    pp_event->u.character.text[i] = key_event->text[i];
    pp_event->u.character.unmodifiedText[i] = key_event->unmodifiedText[i];
  }
}

void BuildMouseEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebMouseEvent* mouse_event =
      reinterpret_cast<const WebMouseEvent*>(event);
  pp_event->u.mouse.modifier = mouse_event->modifiers;
  pp_event->u.mouse.button = mouse_event->button;
  pp_event->u.mouse.x = mouse_event->x;
  pp_event->u.mouse.y = mouse_event->y;
  pp_event->u.mouse.clickCount = mouse_event->clickCount;
}

void BuildMouseWheelEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebMouseWheelEvent* mouse_wheel_event =
      reinterpret_cast<const WebMouseWheelEvent*>(event);
  pp_event->u.wheel.modifier = mouse_wheel_event->modifiers;
  pp_event->u.wheel.deltaX = mouse_wheel_event->deltaX;
  pp_event->u.wheel.deltaY = mouse_wheel_event->deltaY;
  pp_event->u.wheel.wheelTicksX = mouse_wheel_event->wheelTicksX;
  pp_event->u.wheel.wheelTicksY = mouse_wheel_event->wheelTicksY;
  pp_event->u.wheel.scrollByPage = mouse_wheel_event->scrollByPage;
}


WebKeyboardEvent* BuildKeyEvent(const PP_Event& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  switch (event.type) {
    case PP_EVENT_TYPE_RAWKEYDOWN:
      key_event->type = WebInputEvent::RawKeyDown;
      break;
    case PP_EVENT_TYPE_KEYDOWN:
      key_event->type = WebInputEvent::KeyDown;
      break;
    case PP_EVENT_TYPE_KEYUP:
      key_event->type = WebInputEvent::KeyUp;
      break;
  }
  key_event->timeStampSeconds = event.time_stamp_seconds;
  key_event->modifiers = event.u.key.modifier;
  key_event->windowsKeyCode = event.u.key.normalizedKeyCode;
  return key_event;
}

WebKeyboardEvent* BuildCharEvent(const PP_Event& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  key_event->type = WebInputEvent::Char;
  key_event->timeStampSeconds = event.time_stamp_seconds;
  key_event->modifiers = event.u.character.modifier;
  // For consistency, check that the sizes of the texts agree.
  DCHECK(sizeof(event.u.character.text) == sizeof(key_event->text));
  DCHECK(sizeof(event.u.character.unmodifiedText) ==
         sizeof(key_event->unmodifiedText));
  for (size_t i = 0; i < WebKeyboardEvent::textLengthCap; ++i) {
    key_event->text[i] = event.u.character.text[i];
    key_event->unmodifiedText[i] = event.u.character.unmodifiedText[i];
  }
  return key_event;
}

WebMouseEvent* BuildMouseEvent(const PP_Event& event) {
  WebMouseEvent* mouse_event = new WebMouseEvent();
  switch (event.type) {
    case PP_EVENT_TYPE_MOUSEDOWN:
      mouse_event->type = WebInputEvent::MouseDown;
      break;
    case PP_EVENT_TYPE_MOUSEUP:
      mouse_event->type = WebInputEvent::MouseUp;
      break;
    case PP_EVENT_TYPE_MOUSEMOVE:
      mouse_event->type = WebInputEvent::MouseMove;
      break;
    case PP_EVENT_TYPE_MOUSEENTER:
      mouse_event->type = WebInputEvent::MouseEnter;
      break;
    case PP_EVENT_TYPE_MOUSELEAVE:
      mouse_event->type = WebInputEvent::MouseLeave;
      break;
  }
  mouse_event->timeStampSeconds = event.time_stamp_seconds;
  mouse_event->modifiers = event.u.mouse.modifier;
  mouse_event->button =
      static_cast<WebMouseEvent::Button>(event.u.mouse.button);
  mouse_event->x = event.u.mouse.x;
  mouse_event->y = event.u.mouse.y;
  mouse_event->clickCount = event.u.mouse.clickCount;
  return mouse_event;
}

WebMouseWheelEvent* BuildMouseWheelEvent(const PP_Event& event) {
  WebMouseWheelEvent* mouse_wheel_event = new WebMouseWheelEvent();
  mouse_wheel_event->type = WebInputEvent::MouseWheel;
  mouse_wheel_event->timeStampSeconds = event.time_stamp_seconds;
  mouse_wheel_event->modifiers = event.u.wheel.modifier;
  mouse_wheel_event->deltaX = event.u.wheel.deltaX;
  mouse_wheel_event->deltaY = event.u.wheel.deltaY;
  mouse_wheel_event->wheelTicksX = event.u.wheel.wheelTicksX;
  mouse_wheel_event->wheelTicksY = event.u.wheel.wheelTicksY;
  mouse_wheel_event->scrollByPage = event.u.wheel.scrollByPage;
  return mouse_wheel_event;
}

}  // namespace

namespace pepper {

PP_Event* CreatePP_Event(const WebInputEvent& event) {
  scoped_ptr<PP_Event> pp_event(new PP_Event);

  pp_event->type = ConvertEventTypes(event.type);
  pp_event->size = sizeof(pp_event);
  pp_event->time_stamp_seconds = event.timeStampSeconds;
  switch (pp_event->type) {
    case PP_EVENT_TYPE_UNDEFINED:
      return NULL;
    case PP_EVENT_TYPE_MOUSEDOWN:
    case PP_EVENT_TYPE_MOUSEUP:
    case PP_EVENT_TYPE_MOUSEMOVE:
    case PP_EVENT_TYPE_MOUSEENTER:
    case PP_EVENT_TYPE_MOUSELEAVE:
      BuildMouseEvent(&event, pp_event.get());
      break;
    case PP_EVENT_TYPE_MOUSEWHEEL:
      BuildMouseWheelEvent(&event, pp_event.get());
      break;
    case PP_EVENT_TYPE_RAWKEYDOWN:
    case PP_EVENT_TYPE_KEYDOWN:
    case PP_EVENT_TYPE_KEYUP:
      BuildKeyEvent(&event, pp_event.get());
      break;
    case PP_EVENT_TYPE_CHAR:
      BuildCharEvent(&event, pp_event.get());
      break;
  }

  return pp_event.release();
}

WebInputEvent* CreateWebInputEvent(const PP_Event& event) {
  scoped_ptr<WebInputEvent> web_input_event;
  switch (event.type) {
    case PP_EVENT_TYPE_UNDEFINED:
      return NULL;
    case PP_EVENT_TYPE_MOUSEDOWN:
    case PP_EVENT_TYPE_MOUSEUP:
    case PP_EVENT_TYPE_MOUSEMOVE:
    case PP_EVENT_TYPE_MOUSEENTER:
    case PP_EVENT_TYPE_MOUSELEAVE:
      web_input_event.reset(BuildMouseEvent(event));
      break;
    case PP_EVENT_TYPE_MOUSEWHEEL:
      web_input_event.reset(BuildMouseWheelEvent(event));
      break;
    case PP_EVENT_TYPE_RAWKEYDOWN:
    case PP_EVENT_TYPE_KEYDOWN:
    case PP_EVENT_TYPE_KEYUP:
      web_input_event.reset(BuildKeyEvent(event));
      break;
    case PP_EVENT_TYPE_CHAR:
      web_input_event.reset(BuildCharEvent(event));
      break;
    case PP_EVENT_TYPE_FOCUS:
      // NOTIMPLEMENTED();
      return NULL;
  }

  return web_input_event.release();
}

}  // namespace pepper
