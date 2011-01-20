// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/event_conversion.h"

#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/utf_string_conversion_utils.h"
#include "ppapi/c/pp_input_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/plugins/ppapi/common.h"

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

namespace webkit {
namespace ppapi {

namespace {

PP_InputEvent_Type ConvertEventTypes(WebInputEvent::Type wetype) {
  switch (wetype) {
    case WebInputEvent::MouseDown:
      return PP_INPUTEVENT_TYPE_MOUSEDOWN;
    case WebInputEvent::MouseUp:
      return PP_INPUTEVENT_TYPE_MOUSEUP;
    case WebInputEvent::MouseMove:
      return PP_INPUTEVENT_TYPE_MOUSEMOVE;
    case WebInputEvent::MouseEnter:
      return PP_INPUTEVENT_TYPE_MOUSEENTER;
    case WebInputEvent::MouseLeave:
      return PP_INPUTEVENT_TYPE_MOUSELEAVE;
    case WebInputEvent::MouseWheel:
      return PP_INPUTEVENT_TYPE_MOUSEWHEEL;
    case WebInputEvent::RawKeyDown:
      return PP_INPUTEVENT_TYPE_RAWKEYDOWN;
    case WebInputEvent::KeyDown:
      return PP_INPUTEVENT_TYPE_KEYDOWN;
    case WebInputEvent::KeyUp:
      return PP_INPUTEVENT_TYPE_KEYUP;
    case WebInputEvent::Char:
      return PP_INPUTEVENT_TYPE_CHAR;
    case WebInputEvent::Undefined:
    default:
      return PP_INPUTEVENT_TYPE_UNDEFINED;
  }
}

// Generates a PP_InputEvent with the fields common to all events, as well as
// the event type from the given web event. Event-specific fields will be zero
// initialized.
PP_InputEvent GetPPEventWithCommonFieldsAndType(
    const WebInputEvent& web_event) {
  PP_InputEvent result;
  memset(&result, 0, sizeof(PP_InputEvent));
  result.type = ConvertEventTypes(web_event.type);
  // TODO(brettw) http://code.google.com/p/chromium/issues/detail?id=57448
  // This should use a tick count rather than the wall clock time that WebKit
  // uses.
  result.time_stamp = web_event.timeStampSeconds;
  return result;
}

void AppendKeyEvent(const WebInputEvent& event,
                    std::vector<PP_InputEvent>* pp_events) {
  const WebKeyboardEvent& key_event =
      reinterpret_cast<const WebKeyboardEvent&>(event);
  PP_InputEvent result = GetPPEventWithCommonFieldsAndType(event);
  result.u.key.modifier = key_event.modifiers;
  result.u.key.key_code = key_event.windowsKeyCode;
  pp_events->push_back(result);
}

void AppendCharEvent(const WebInputEvent& event,
                     std::vector<PP_InputEvent>* pp_events) {
  const WebKeyboardEvent& key_event =
      reinterpret_cast<const WebKeyboardEvent&>(event);

  // This is a bit complex, the input event will normally just have one 16-bit
  // character in it, but may be zero or more than one. The text array is
  // just padded with 0 values for the unused ones, but is not necessarily
  // null-terminated.
  //
  // Here we see how many UTF-16 characters we have.
  size_t utf16_char_count = 0;
  while (utf16_char_count < WebKeyboardEvent::textLengthCap &&
         key_event.text[utf16_char_count])
    utf16_char_count++;

  // Make a separate PP_InputEvent for each Unicode character in the input.
  base::i18n::UTF16CharIterator iter(key_event.text, utf16_char_count);
  while (!iter.end()) {
    PP_InputEvent result = GetPPEventWithCommonFieldsAndType(event);
    result.u.character.modifier = key_event.modifiers;

    std::string utf8_char;
    base::WriteUnicodeCharacter(iter.get(), &utf8_char);
    base::strlcpy(result.u.character.text, utf8_char.c_str(),
                  sizeof(result.u.character.text));

    pp_events->push_back(result);
    iter.Advance();
  }
}

void AppendMouseEvent(const WebInputEvent& event,
                      std::vector<PP_InputEvent>* pp_events) {
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonNone) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_NONE),
                 MouseNone);
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonLeft) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_LEFT),
                 MouseLeft);
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonRight) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_RIGHT),
                 MouseRight);
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonMiddle) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_MIDDLE),
                 MouseMiddle);

  const WebMouseEvent& mouse_event =
      reinterpret_cast<const WebMouseEvent&>(event);
  PP_InputEvent result = GetPPEventWithCommonFieldsAndType(event);
  result.u.mouse.modifier = mouse_event.modifiers;
  result.u.mouse.button =
      static_cast<PP_InputEvent_MouseButton>(mouse_event.button);
  result.u.mouse.x = static_cast<float>(mouse_event.x);
  result.u.mouse.y = static_cast<float>(mouse_event.y);
  result.u.mouse.click_count = mouse_event.clickCount;
  pp_events->push_back(result);
}

void AppendMouseWheelEvent(const WebInputEvent& event,
                           std::vector<PP_InputEvent>* pp_events) {
  const WebMouseWheelEvent& mouse_wheel_event =
      reinterpret_cast<const WebMouseWheelEvent&>(event);
  PP_InputEvent result = GetPPEventWithCommonFieldsAndType(event);
  result.u.wheel.modifier = mouse_wheel_event.modifiers;
  result.u.wheel.delta_x = mouse_wheel_event.deltaX;
  result.u.wheel.delta_y = mouse_wheel_event.deltaY;
  result.u.wheel.wheel_ticks_x = mouse_wheel_event.wheelTicksX;
  result.u.wheel.wheel_ticks_y = mouse_wheel_event.wheelTicksY;
  result.u.wheel.scroll_by_page =
      BoolToPPBool(!!mouse_wheel_event.scrollByPage);
  pp_events->push_back(result);
}

WebKeyboardEvent* BuildKeyEvent(const PP_InputEvent& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  switch (event.type) {
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      key_event->type = WebInputEvent::RawKeyDown;
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      key_event->type = WebInputEvent::KeyDown;
      break;
    case PP_INPUTEVENT_TYPE_KEYUP:
      key_event->type = WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }
  key_event->timeStampSeconds = event.time_stamp;
  key_event->modifiers = event.u.key.modifier;
  key_event->windowsKeyCode = event.u.key.key_code;
  return key_event;
}

WebKeyboardEvent* BuildCharEvent(const PP_InputEvent& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  key_event->type = WebInputEvent::Char;
  key_event->timeStampSeconds = event.time_stamp;
  key_event->modifiers = event.u.character.modifier;

  // Make sure to not read beyond the buffer in case some bad code doesn't
  // NULL-terminate it (this is called from plugins).
  size_t text_length_cap = WebKeyboardEvent::textLengthCap;
  size_t text_len = 0;
  while (text_len < text_length_cap && event.u.character.text[text_len])
      text_len++;
  string16 text16 = UTF8ToUTF16(std::string(event.u.character.text, text_len));

  memset(key_event->text, 0, text_length_cap);
  memset(key_event->unmodifiedText, 0, text_length_cap);
  for (size_t i = 0;
       i < std::min(text_length_cap, text16.size());
       ++i)
    key_event->text[i] = text16[i];
  return key_event;
}

WebMouseEvent* BuildMouseEvent(const PP_InputEvent& event) {
  WebMouseEvent* mouse_event = new WebMouseEvent();
  switch (event.type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      mouse_event->type = WebInputEvent::MouseDown;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      mouse_event->type = WebInputEvent::MouseUp;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      mouse_event->type = WebInputEvent::MouseMove;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      mouse_event->type = WebInputEvent::MouseEnter;
      break;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      mouse_event->type = WebInputEvent::MouseLeave;
      break;
    default:
      NOTREACHED();
  }
  mouse_event->timeStampSeconds = event.time_stamp;
  mouse_event->modifiers = event.u.mouse.modifier;
  mouse_event->button =
      static_cast<WebMouseEvent::Button>(event.u.mouse.button);
  mouse_event->x = static_cast<int>(event.u.mouse.x);
  mouse_event->y = static_cast<int>(event.u.mouse.y);
  mouse_event->clickCount = event.u.mouse.click_count;
  return mouse_event;
}

WebMouseWheelEvent* BuildMouseWheelEvent(const PP_InputEvent& event) {
  WebMouseWheelEvent* mouse_wheel_event = new WebMouseWheelEvent();
  mouse_wheel_event->type = WebInputEvent::MouseWheel;
  mouse_wheel_event->timeStampSeconds = event.time_stamp;
  mouse_wheel_event->modifiers = event.u.wheel.modifier;
  mouse_wheel_event->deltaX = event.u.wheel.delta_x;
  mouse_wheel_event->deltaY = event.u.wheel.delta_y;
  mouse_wheel_event->wheelTicksX = event.u.wheel.wheel_ticks_x;
  mouse_wheel_event->wheelTicksY = event.u.wheel.wheel_ticks_y;
  mouse_wheel_event->scrollByPage = event.u.wheel.scroll_by_page;
  return mouse_wheel_event;
}

}  // namespace

void CreatePPEvent(const WebInputEvent& event,
                   std::vector<PP_InputEvent>* pp_events) {
  pp_events->clear();

  switch (event.type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseLeave:
      AppendMouseEvent(event, pp_events);
      break;
    case WebInputEvent::MouseWheel:
      AppendMouseWheelEvent(event, pp_events);
      break;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      AppendKeyEvent(event, pp_events);
      break;
    case WebInputEvent::Char:
      AppendCharEvent(event, pp_events);
      break;
    case WebInputEvent::Undefined:
    default:
      break;
  }
}

WebInputEvent* CreateWebInputEvent(const PP_InputEvent& event) {
  scoped_ptr<WebInputEvent> web_input_event;
  switch (event.type) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return NULL;
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      web_input_event.reset(BuildMouseEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_MOUSEWHEEL:
      web_input_event.reset(BuildMouseWheelEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
      web_input_event.reset(BuildKeyEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_CHAR:
      web_input_event.reset(BuildCharEvent(event));
      break;
  }

  return web_input_event.release();
}
}  // namespace ppapi
}  // namespace webkit

