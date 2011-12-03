// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/event_conversion.h"

#include "base/basictypes.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/utf_string_conversion_utils.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/plugins/ppapi/common.h"

using ppapi::EventTimeToPPTimeTicks;
using ppapi::InputEventData;
using ppapi::PPTimeTicksToEventTime;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebString;
using WebKit::WebUChar;

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
    case WebInputEvent::ContextMenu:
      return PP_INPUTEVENT_TYPE_CONTEXTMENU;
    case WebInputEvent::MouseWheel:
      return PP_INPUTEVENT_TYPE_WHEEL;
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
InputEventData GetEventWithCommonFieldsAndType(const WebInputEvent& web_event) {
  InputEventData result;
  result.event_type = ConvertEventTypes(web_event.type);
  result.event_time_stamp = EventTimeToPPTimeTicks(web_event.timeStampSeconds);
  return result;
}

void AppendKeyEvent(const WebInputEvent& event,
                    std::vector<InputEventData>* result_events) {
  const WebKeyboardEvent& key_event =
      static_cast<const WebKeyboardEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = key_event.modifiers;
  result.key_code = key_event.windowsKeyCode;
  result_events->push_back(result);
}

void AppendCharEvent(const WebInputEvent& event,
                     std::vector<InputEventData>* result_events) {
  const WebKeyboardEvent& key_event =
      static_cast<const WebKeyboardEvent&>(event);

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

  // Make a separate InputEventData for each Unicode character in the input.
  base::i18n::UTF16CharIterator iter(key_event.text, utf16_char_count);
  while (!iter.end()) {
    InputEventData result = GetEventWithCommonFieldsAndType(event);
    result.event_modifiers = key_event.modifiers;
    base::WriteUnicodeCharacter(iter.get(), &result.character_text);

    result_events->push_back(result);
    iter.Advance();
  }
}

void AppendMouseEvent(const WebInputEvent& event,
                      std::vector<InputEventData>* result_events) {
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
      static_cast<const WebMouseEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = mouse_event.modifiers;
  if (mouse_event.type == WebInputEvent::MouseDown ||
      mouse_event.type == WebInputEvent::MouseUp) {
    result.mouse_button =
        static_cast<PP_InputEvent_MouseButton>(mouse_event.button);
  }
  result.mouse_position.x = mouse_event.x;
  result.mouse_position.y = mouse_event.y;
  result.mouse_click_count = mouse_event.clickCount;
  result.mouse_movement.x = mouse_event.movementX;
  result.mouse_movement.y = mouse_event.movementY;
  result_events->push_back(result);
}

void AppendMouseWheelEvent(const WebInputEvent& event,
                           std::vector<InputEventData>* result_events) {
  const WebMouseWheelEvent& mouse_wheel_event =
      static_cast<const WebMouseWheelEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = mouse_wheel_event.modifiers;
  result.wheel_delta.x = mouse_wheel_event.deltaX;
  result.wheel_delta.y = mouse_wheel_event.deltaY;
  result.wheel_ticks.x = mouse_wheel_event.wheelTicksX;
  result.wheel_ticks.y = mouse_wheel_event.wheelTicksY;
  result.wheel_scroll_by_page = !!mouse_wheel_event.scrollByPage;
  result_events->push_back(result);
}

WebKeyboardEvent* BuildKeyEvent(const InputEventData& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  switch (event.event_type) {
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
  key_event->timeStampSeconds = PPTimeTicksToEventTime(event.event_time_stamp);
  key_event->modifiers = event.event_modifiers;
  key_event->windowsKeyCode = event.key_code;
  return key_event;
}

WebKeyboardEvent* BuildCharEvent(const InputEventData& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  key_event->type = WebInputEvent::Char;
  key_event->timeStampSeconds = PPTimeTicksToEventTime(event.event_time_stamp);
  key_event->modifiers = event.event_modifiers;

  // Make sure to not read beyond the buffer in case some bad code doesn't
  // NULL-terminate it (this is called from plugins).
  size_t text_length_cap = WebKeyboardEvent::textLengthCap;
  string16 text16 = UTF8ToUTF16(event.character_text);

  memset(key_event->text, 0, text_length_cap);
  memset(key_event->unmodifiedText, 0, text_length_cap);
  for (size_t i = 0;
       i < std::min(text_length_cap, text16.size());
       ++i)
    key_event->text[i] = text16[i];
  return key_event;
}

WebMouseEvent* BuildMouseEvent(const InputEventData& event) {
  WebMouseEvent* mouse_event = new WebMouseEvent();
  switch (event.event_type) {
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
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      mouse_event->type = WebInputEvent::ContextMenu;
      break;
    default:
      NOTREACHED();
  }
  mouse_event->timeStampSeconds =
      PPTimeTicksToEventTime(event.event_time_stamp);
  mouse_event->modifiers = event.event_modifiers;
  mouse_event->button =
      static_cast<WebMouseEvent::Button>(event.mouse_button);
  mouse_event->x = event.mouse_position.x;
  mouse_event->y = event.mouse_position.y;
  mouse_event->clickCount = event.mouse_click_count;
  mouse_event->movementX = event.mouse_movement.x;
  mouse_event->movementY = event.mouse_movement.y;
  return mouse_event;
}

WebMouseWheelEvent* BuildMouseWheelEvent(const InputEventData& event) {
  WebMouseWheelEvent* mouse_wheel_event = new WebMouseWheelEvent();
  mouse_wheel_event->type = WebInputEvent::MouseWheel;
  mouse_wheel_event->timeStampSeconds =
      PPTimeTicksToEventTime(event.event_time_stamp);
  mouse_wheel_event->modifiers = event.event_modifiers;
  mouse_wheel_event->deltaX = event.wheel_delta.x;
  mouse_wheel_event->deltaY = event.wheel_delta.y;
  mouse_wheel_event->wheelTicksX = event.wheel_ticks.x;
  mouse_wheel_event->wheelTicksY = event.wheel_ticks.y;
  mouse_wheel_event->scrollByPage = event.wheel_scroll_by_page;
  return mouse_wheel_event;
}

#if !defined(OS_WIN)
#define VK_RETURN         0x0D

#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E

#define VK_APPS           0x5D

#define VK_F1             0x70
#endif

// Convert a character string to a Windows virtual key code. Adapted from
// src/third_party/WebKit/Tools/DumpRenderTree/chromium/EventSender.cpp. This
// is used by CreateSimulatedWebInputEvents to convert keyboard events.
void GetKeyCode(const std::string& char_text,
                WebUChar* code,
                WebUChar* text,
                bool* needs_shift_modifier,
                bool* generate_char) {
  WebUChar vk_code = 0;
  WebUChar vk_text = 0;
  *needs_shift_modifier = false;
  *generate_char = false;
  if ("\n" == char_text) {
    vk_text = vk_code = VK_RETURN;
    *generate_char = true;
  } else if ("rightArrow" == char_text) {
    vk_code = VK_RIGHT;
  } else if ("downArrow" == char_text) {
    vk_code = VK_DOWN;
  } else if ("leftArrow" == char_text) {
    vk_code = VK_LEFT;
  } else if ("upArrow" == char_text) {
    vk_code = VK_UP;
  } else if ("insert" == char_text) {
    vk_code = VK_INSERT;
  } else if ("delete" == char_text) {
    vk_code = VK_DELETE;
  } else if ("pageUp" == char_text) {
    vk_code = VK_PRIOR;
  } else if ("pageDown" == char_text) {
    vk_code = VK_NEXT;
  } else if ("home" == char_text) {
    vk_code = VK_HOME;
  } else if ("end" == char_text) {
    vk_code = VK_END;
  } else if ("printScreen" == char_text) {
    vk_code = VK_SNAPSHOT;
  } else if ("menu" == char_text) {
    vk_code = VK_APPS;
  } else {
    // Compare the input string with the function-key names defined by the
    // DOM spec (i.e. "F1",...,"F24").
    for (int i = 1; i <= 24; ++i) {
      std::string functionKeyName = base::StringPrintf("F%d", i);
      if (functionKeyName == char_text) {
        vk_code = VK_F1 + (i - 1);
        break;
      }
    }
    if (!vk_code) {
      WebString web_char_text =
          WebString::fromUTF8(char_text.data(), char_text.size());
      DCHECK_EQ(web_char_text.length(), 1U);
      vk_text = vk_code = web_char_text.data()[0];
      *needs_shift_modifier =
          (vk_code & 0xFF) >= 'A' && (vk_code & 0xFF) <= 'Z';
      if ((vk_code & 0xFF) >= 'a' && (vk_code & 0xFF) <= 'z')
          vk_code -= 'a' - 'A';
      *generate_char = true;
    }
  }

  *code = vk_code;
  *text = vk_text;
}

}  // namespace

void CreateInputEventData(const WebInputEvent& event,
                          std::vector<InputEventData>* result) {
  result->clear();

  switch (event.type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::ContextMenu:
      AppendMouseEvent(event, result);
      break;
    case WebInputEvent::MouseWheel:
      AppendMouseWheelEvent(event, result);
      break;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      AppendKeyEvent(event, result);
      break;
    case WebInputEvent::Char:
      AppendCharEvent(event, result);
      break;
    case WebInputEvent::Undefined:
    default:
      break;
  }
}

WebInputEvent* CreateWebInputEvent(const InputEventData& event) {
  scoped_ptr<WebInputEvent> web_input_event;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return NULL;
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      web_input_event.reset(BuildMouseEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_WHEEL:
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
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      // TODO(kinaba) implement in WebKit an event structure to handle
      // composition events.
      NOTREACHED();
      break;
  }

  return web_input_event.release();
}

// Generate a coherent sequence of input events to simulate a user event.
// From src/third_party/WebKit/Tools/DumpRenderTree/chromium/EventSender.cpp.
std::vector<linked_ptr<WebInputEvent> > CreateSimulatedWebInputEvents(
    const ::ppapi::InputEventData& event,
    int plugin_x,
    int plugin_y) {
  std::vector<linked_ptr<WebInputEvent> > events;
  linked_ptr<WebInputEvent> original_event(CreateWebInputEvent(event));

  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      events.push_back(original_event);
      break;

    case PP_INPUTEVENT_TYPE_WHEEL: {
      WebMouseWheelEvent* web_mouse_wheel_event =
          static_cast<WebMouseWheelEvent*>(original_event.get());
      web_mouse_wheel_event->x = plugin_x;
      web_mouse_wheel_event->y = plugin_y;
      events.push_back(original_event);
      break;
    }

    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP: {
      // Windows key down events should always be "raw" to avoid an ASSERT.
#if defined(OS_WIN)
      WebKeyboardEvent* web_keyboard_event =
          static_cast<WebKeyboardEvent*>(original_event.get());
      if (web_keyboard_event->type == WebInputEvent::KeyDown)
        web_keyboard_event->type = WebInputEvent::RawKeyDown;
#endif
      events.push_back(original_event);
      break;
    }

    case PP_INPUTEVENT_TYPE_CHAR: {
      WebKeyboardEvent* web_char_event =
          static_cast<WebKeyboardEvent*>(original_event.get());

      WebUChar code = 0, text = 0;
      bool needs_shift_modifier = false, generate_char = false;
      GetKeyCode(event.character_text,
                 &code,
                 &text,
                 &needs_shift_modifier,
                 &generate_char);

      // Synthesize key down and key up events in all cases.
      scoped_ptr<WebKeyboardEvent> key_down_event(new WebKeyboardEvent());
      scoped_ptr<WebKeyboardEvent> key_up_event(new WebKeyboardEvent());

      key_down_event->type = WebInputEvent::RawKeyDown;
      key_down_event->windowsKeyCode = code;
      key_down_event->nativeKeyCode = code;
      if (needs_shift_modifier)
        key_down_event->modifiers |= WebInputEvent::ShiftKey;

      // If a char event is needed, set the text fields.
      if (generate_char) {
        key_down_event->text[0] = text;
        key_down_event->unmodifiedText[0] = text;
      }
      // Convert the key code to a string identifier.
      key_down_event->setKeyIdentifierFromWindowsKeyCode();

      *key_up_event = *web_char_event = *key_down_event;

      events.push_back(linked_ptr<WebInputEvent>(key_down_event.release()));

      if (generate_char) {
        web_char_event->type = WebInputEvent::Char;
        web_char_event->keyIdentifier[0] = '\0';
        events.push_back(original_event);
      }

      key_up_event->type = WebInputEvent::KeyUp;
      events.push_back(linked_ptr<WebInputEvent>(key_up_event.release()));
      break;
    }

    default:
      break;
  }
  return events;
}

PP_InputEvent_Class ClassifyInputEvent(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::ContextMenu:
      return PP_INPUTEVENT_CLASS_MOUSE;
    case WebInputEvent::MouseWheel:
      return PP_INPUTEVENT_CLASS_WHEEL;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
      return PP_INPUTEVENT_CLASS_KEYBOARD;
    case WebInputEvent::Undefined:
    default:
      NOTREACHED();
      return PP_InputEvent_Class(0);
  }
}

}  // namespace ppapi
}  // namespace webkit
