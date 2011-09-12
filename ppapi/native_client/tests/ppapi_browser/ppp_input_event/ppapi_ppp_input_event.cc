// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <vector>

#include "native_client/src/include/nacl/nacl_inttypes.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppp_input_event.h"

namespace {

// These define whether we want an input event to bubble up to the browser
static enum {
  BUBBLE_UNDEFINED,
  BUBBLE_YES,
  BUBBLE_NO
} bubbleInputEvent;

PP_TimeTicks last_time_tick;

std::string InputEventTypeToString(PP_InputEvent_Type type) {
  switch (type) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return "PP_INPUTEVENT_TYPE_UNDEFINED";
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      return "PP_INPUTEVENT_TYPE_MOUSEDOWN";
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      return "PP_INPUTEVENT_TYPE_MOUSEUP";
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      return "PP_INPUTEVENT_TYPE_MOUSEMOVE";
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      return "PP_INPUTEVENT_TYPE_MOUSEENTER";
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      return "PP_INPUTEVENT_TYPE_MOUSELEAVE";
    case PP_INPUTEVENT_TYPE_WHEEL:
      return "PP_INPUTEVENT_TYPE_WHEEL";
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      return "PP_INPUTEVENT_TYPE_RAWKEYDOWN";
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      return "PP_INPUTEVENT_TYPE_KEYDOWN";
    case PP_INPUTEVENT_TYPE_KEYUP:
      return "PP_INPUTEVENT_TYPE_KEYUP";
    case PP_INPUTEVENT_TYPE_CHAR:
      return "PP_INPUTEVENT_TYPE_CHAR";
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      return "PP_INPUTEVENT_TYPE_CONTEXTMENU";
    default:
      std::ostringstream stream;
      stream << "Unknown Input Event: " << type;
      return stream.str();
  }
}

std::string InputEventModifierToString(uint32_t modifier) {
  struct ModifierStringMap {
    PP_InputEvent_Modifier modifier;
    const char* name;
  };
  static const ModifierStringMap table[] = {
    { PP_INPUTEVENT_MODIFIER_SHIFTKEY, "PP_INPUTEVENT_MODIFIER_SHIFTKEY" },
    { PP_INPUTEVENT_MODIFIER_CONTROLKEY, "PP_INPUTEVENT_MODIFIER_CONTROLKEY" },
    { PP_INPUTEVENT_MODIFIER_ALTKEY, "PP_INPUTEVENT_MODIFIER_ALTKEY" },
    { PP_INPUTEVENT_MODIFIER_METAKEY, "PP_INPUTEVENT_MODIFIER_METAKEY" },
    { PP_INPUTEVENT_MODIFIER_ISKEYPAD, "PP_INPUTEVENT_MODIFIER_ISKEYPAD" },
    { PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT,
        "PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT" },
    { PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN,
        "PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN" },
    { PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN,
        "PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN" },
    { PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN,
        "PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN" },
    { PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY,
        "PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY" },
    { PP_INPUTEVENT_MODIFIER_NUMLOCKKEY, "PP_INPUTEVENT_MODIFIER_NUMLOCKKEY" },
  };

  std::ostringstream stream;
  for (int i = 0; i < int(sizeof(table) / sizeof(*table)); ++i) {
    if (table[i].modifier & modifier) {
      if (stream.str().size() != 0) {
        stream << ",";
      }
      stream << table[i].name;
      modifier &= ~table[i].modifier;
    }
  }

  if (modifier != 0) {
    if (stream.str().size() != 0) {
      stream << ",";
    }
    stream << "Unknown Modifier Bits:" << modifier;
  }
  return stream.str();
}

std::string MouseButtonToString(PP_InputEvent_MouseButton button) {
  switch (button) {
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
      return "PP_INPUTEVENT_MOUSEBUTTON_NONE";
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return "PP_INPUTEVENT_MOUSEBUTTON_LEFT";
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return "PP_INPUTEVENT_MOUSEBUTTON_MIDDLE";
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return "PP_INPUTEVENT_MOUSEBUTTON_RIGHT";
    default:
      std::ostringstream stream;
      stream << "Unrecognized ("
             << static_cast<int32_t>(button)
             << ")";
      return stream.str();
  }
}

PP_Bool HandleInputEvent(PP_Instance instance, PP_Resource event) {
  EXPECT(instance == pp_instance());
  EXPECT(PPBInputEvent()->IsInputEvent(event));
  PP_InputEvent_Type type = PPBInputEvent()->GetType(event);
  PP_TimeTicks ticks = PPBInputEvent()->GetTimeStamp(event);
  EXPECT(ticks >= last_time_tick);
  last_time_tick = ticks;
  uint32_t modifiers = PPBInputEvent()->GetModifiers(event);
  printf("--- PPP_InputEvent::HandleInputEvent "
         "type=%d, ticks=%f, modifiers=%"NACL_PRIu32"\n",
         static_cast<int>(type), ticks, modifiers);
  std::ostringstream message;
  message << InputEventTypeToString(type) << ":" <<
      InputEventModifierToString(modifiers);
  if (PPBMouseInputEvent()->IsMouseInputEvent(event)) {
    PP_Point point = PPBMouseInputEvent()->GetPosition(event);
    message << ":" << MouseButtonToString(
        PPBMouseInputEvent()->GetButton(event));
    message << ":Position=" << point.x << "," << point.y;
    message << ":ClickCount=" << PPBMouseInputEvent()->GetClickCount(event);
  }
  // Here's a example of what a complete response would look like for a mouse
  // click (broken into multiple lines for readability):
  // HandleInputEvent:PP_INPUTEVENT_TYPE_MOUSEDOWN
  // :PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN
  // :PP_INPUTEVENT_MOUSEBUTTON_LEFT
  // :Position=0,0:ClickCount=1
  // TODO(mball) Add output for WheelInputEvent and KeyboardInputEvent

  PostTestMessage("HandleInputEvent", message.str());
  EXPECT(bubbleInputEvent == BUBBLE_YES || bubbleInputEvent == BUBBLE_NO);
  return (bubbleInputEvent == BUBBLE_NO) ? PP_TRUE : PP_FALSE;
}

void TestUnfilteredMouseEvents() {
  EXPECT(PPBInputEvent()->RequestInputEvents(
         pp_instance(),
         PP_INPUTEVENT_CLASS_MOUSE) == PP_OK);
  bubbleInputEvent = BUBBLE_NO;
  TEST_PASSED;
}

void TestBubbledMouseEvents() {
  EXPECT(PPBInputEvent()->RequestFilteringInputEvents(
         pp_instance(),
         PP_INPUTEVENT_CLASS_MOUSE) == PP_OK);
  bubbleInputEvent = BUBBLE_YES;
  TEST_PASSED;
}

void TestUnbubbledMouseEvents() {
  EXPECT(PPBInputEvent()->RequestFilteringInputEvents(
         pp_instance(),
         PP_INPUTEVENT_CLASS_MOUSE) == PP_OK);
  bubbleInputEvent = BUBBLE_NO;
  TEST_PASSED;
}

void Cleanup() {
  PPBInputEvent()->ClearInputEventRequest(
         pp_instance(),
         PP_INPUTEVENT_CLASS_MOUSE |
         PP_INPUTEVENT_CLASS_WHEEL |
         PP_INPUTEVENT_CLASS_KEYBOARD);
  bubbleInputEvent = BUBBLE_UNDEFINED;
  TEST_PASSED;
}

const PPP_InputEvent ppp_input_event_interface = {
    &HandleInputEvent
};

}  // namespace

void SetupTests() {
  bubbleInputEvent = BUBBLE_UNDEFINED;
  last_time_tick = 0.0;
  RegisterTest("TestUnfilteredMouseEvents", TestUnfilteredMouseEvents);
  RegisterTest("TestBubbledMouseEvents", TestBubbledMouseEvents);
  RegisterTest("TestUnbubbledMouseEvents", TestUnbubbledMouseEvents);
  RegisterTest("Cleanup", Cleanup);
}

void SetupPluginInterfaces() {
  RegisterPluginInterface(PPP_INPUT_EVENT_INTERFACE,
                          &ppp_input_event_interface);
}
