// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C headers
#include <cassert>
#include <cstdio>

// C++ headers
#include <sstream>
#include <string>

// NaCl
#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace {
const char* const kEventsPropertyName = "events";

// Convert a given modifier to a descriptive string.  Note that the actual
// declared type of modifier in each of the event structs (e.g.,
// PP_InputEvent_Key.modifier) is uint32_t, but it is expected to be
// interpreted as a bitfield of 'or'ed PP_InputEvent_Modifier values.
std::string ModifierToString(uint32_t modifier) {
  std::string s;
  if (modifier & PP_INPUTEVENT_MODIFIER_SHIFTKEY) {
    s += "shift ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_CONTROLKEY) {
    s += "ctrl ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ALTKEY) {
    s += "alt ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_METAKEY) {
    s += "meta ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ISKEYPAD) {
    s += "keypad ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT) {
    s += "autorepeat ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
    s += "left-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN) {
    s += "middle-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN) {
    s += "right-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY) {
    s += "caps-lock ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_NUMLOCKKEY) {
    s += "num-lock ";
  }
  return s;
}

std::string MouseButtonToString(PP_InputEvent_MouseButton button) {
  switch (button) {
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
      return "None";
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return "Left";
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return "Middle";
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return "Right";
    default:
      std::ostringstream stream;
      stream << "Unrecognized ("
             << static_cast<int32_t>(button)
             << ")";
      return stream.str();
  }
}

}  // namespace

class EventInstance : public pp::Instance {
 public:
  explicit EventInstance(PP_Instance instance)
      : pp::Instance(instance) {
    std::printf("EventInstance created.\n");
  }
  virtual ~EventInstance() {}

  void KeyEvent(PP_InputEvent_Key key,
                PP_TimeTicks time,
                const std::string& kind) {
    std::ostringstream stream;
    stream << pp_instance() << ":"
           << " Key event:" << kind.c_str()
           << " modifier:" << ModifierToString(key.modifier)
           << " key_code:" << key.key_code
           << " time:" << time
           << "\n";
    std::printf("%s", stream.str().c_str());
    PostMessage(stream.str());
  }

  void MouseEvent(PP_InputEvent_Mouse mouse_event,
                  PP_TimeTicks time,
                  const std::string& kind) {
    std::ostringstream stream;
    stream << pp_instance() << ":"
           << " Mouse event:" << kind.c_str()
           << " modifier:" << ModifierToString(mouse_event.modifier).c_str()
           << " button:" << MouseButtonToString(mouse_event.button).c_str()
           << " x:" << mouse_event.x
           << " y:" << mouse_event.y
           << " click_count:" << mouse_event.click_count
           << " time:" << time
           << "\n";
    std::printf("%s", stream.str().c_str());
    PostMessage(stream.str());
  }

  void CharEvent(PP_InputEvent_Character char_event, PP_TimeTicks time) {
    std::ostringstream stream;
    stream << pp_instance() << ": Character event."
           << " modifier:" << ModifierToString(char_event.modifier).c_str()
           << " text:" << char_event.text
           << " time:" << time
           << "\n";
    std::printf("%s", stream.str().c_str());
    PostMessage(stream.str());
  }

  void WheelEvent(PP_InputEvent_Wheel wheel_event, PP_TimeTicks time) {
    std::ostringstream stream;
    stream << pp_instance() << ": Wheel event."
           << " modifier:" << ModifierToString(wheel_event.modifier).c_str()
           << " deltax:" << wheel_event.delta_x
           << " deltay:" << wheel_event.delta_y
           << " wheel_ticks_x:" << wheel_event.wheel_ticks_x
           << " wheel_ticks_y:" << wheel_event.wheel_ticks_y
           << " scroll_by_page:"
           << (wheel_event.scroll_by_page ? "true" : "false")
           << "\n";
    std::printf("%s", stream.str().c_str());
    PostMessage(stream.str());
  }

  // Handle an incoming input event by switching on type and dispatching
  // to the appropriate subtype handler.
  virtual bool HandleInputEvent(const PP_InputEvent& event) {
    std::printf("HandleInputEvent called\n");
    switch (event.type) {
      case PP_INPUTEVENT_TYPE_UNDEFINED:
        std::printf("Undefined event.\n");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        MouseEvent(event.u.mouse, event.time_stamp, "Down");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEUP:
        MouseEvent(event.u.mouse, event.time_stamp, "Up");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
        MouseEvent(event.u.mouse, event.time_stamp, "Move");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEENTER:
        MouseEvent(event.u.mouse, event.time_stamp, "Enter");
        break;
      case PP_INPUTEVENT_TYPE_MOUSELEAVE:
        MouseEvent(event.u.mouse, event.time_stamp, "Leave");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEWHEEL:
        WheelEvent(event.u.wheel, event.time_stamp);
        break;
      case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
        KeyEvent(event.u.key, event.time_stamp, "RawKeyDown");
        break;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        KeyEvent(event.u.key, event.time_stamp, "Down");
        break;
      case PP_INPUTEVENT_TYPE_KEYUP:
        KeyEvent(event.u.key, event.time_stamp, "Up");
        break;
      case PP_INPUTEVENT_TYPE_CHAR:
        CharEvent(event.u.character, event.time_stamp);
        break;
      default:
        std::printf("Unrecognized event type: %d\n", event.type);
        assert(false);
        return false;
    }
    return true;
  }
};

// The EventModule provides an implementation of pp::Module that creates
// EventInstance objects when invoked.  This is part of the glue code that makes
// our example accessible to ppapi.
class EventModule : public pp::Module {
 public:
  EventModule() : pp::Module() {}
  virtual ~EventModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    std::printf("Creating EventInstance.\n");
    return new EventInstance(instance);
  }
};

// Implement the required pp::CreateModule function that creates our specific
// kind of Module (in this case, EventModule).  This is part of the glue code
// that makes our example accessible to ppapi.
namespace pp {
  Module* CreateModule() {
    std::printf("Creating EventModule.\n");
    return new EventModule();
  }
}
