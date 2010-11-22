// Copyright (c) 2010 The Native Client Authors. All rights reserved.
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
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace {
const char* const kEventsPropertyName = "events";
}  // namespace

// This is the class to which EventInstance delegates event handling.  For each
// input event, we append a description of the event on to events_ and then
// printf the event we just received.  We expose events_ as a JavaScript
// property named "events".  Each time this property is accessed, we clear it
// so that future requests for "events" will only include events that happened
// since the last request for "events".
class EventHandler : public pp::deprecated::ScriptableObject {
 public:
  explicit EventHandler(int64_t instance)
      : instance_(instance),
        events_() {
    std::printf("EventHandler created.\n");
  }
  ~EventHandler() {
    std::printf("EventHandler destroyed.\n");
  }

  virtual bool HasProperty(const pp::Var& property_name, pp::Var* exception) {
    return (property_name.is_string() &&
            (property_name.AsString() == kEventsPropertyName));
  }

  virtual pp::Var GetProperty(const pp::Var& property_name,
                              pp::Var* exception) {
    if (property_name.is_string() &&
        (property_name.AsString() == kEventsPropertyName)) {
      std::string string_to_return(events_);
      events_.clear();
      return pp::Var(string_to_return);
    }
    *exception = pp::Var(std::string("No property named ") +
                         property_name.AsString() + " found.");
    return pp::Var();
  }

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

  void KeyEvent(PP_InputEvent_Key key,
                PP_TimeTicks time,
                const std::string& kind) {
    std::ostringstream stream;
    stream << instance_ << ":"
           << " Key event:" << kind.c_str()
           << " modifier:" << ModifierToString(key.modifier)
           << " key_code:" << key.key_code
           << " time:" << time
           << "\n";
    events_ += stream.str();
    std::printf("%s", stream.str().c_str());
  }

  void MouseEvent(PP_InputEvent_Mouse mouse_event,
                  PP_TimeTicks time,
                  const std::string& kind) {
    std::ostringstream stream;
    stream << instance_ << ":"
           << " Mouse event:" << kind.c_str()
           << " modifier:" << ModifierToString(mouse_event.modifier).c_str()
           << " button:" << MouseButtonToString(mouse_event.button).c_str()
           << " x:" << mouse_event.x
           << " y:" << mouse_event.y
           << " click_count:" << mouse_event.click_count
           << " time:" << time
           << "\n";
    events_ += stream.str();
    std::printf("%s", stream.str().c_str());
  }

  void CharEvent(PP_InputEvent_Character char_event, PP_TimeTicks time) {
    std::ostringstream stream;
    stream << instance_ << ": Character event."
           << " modifier:" << ModifierToString(char_event.modifier).c_str()
           << " text:" << char_event.text
           << " time:" << time
           << "\n";
    events_ += stream.str();
    std::printf("%s", stream.str().c_str());
  }

  void WheelEvent(PP_InputEvent_Wheel wheel_event, PP_TimeTicks time) {
    std::ostringstream stream;
    stream << instance_ << ": Wheel event."
           << " modifier:" << ModifierToString(wheel_event.modifier).c_str()
           << " deltax:" << wheel_event.delta_x
           << " deltay:" << wheel_event.delta_y
           << " wheel_ticks_x:" << wheel_event.wheel_ticks_x
           << " wheel_ticks_y:" << wheel_event.wheel_ticks_y
           << " scroll_by_page:"
           << (wheel_event.scroll_by_page ? "true" : "false")
           << "\n";
    events_ += stream.str();
    std::printf("%s", stream.str().c_str());
  }


 private:
  // The resource number of the pp::Instance that created us (for use in
  // output).
  int64_t instance_;
  // A string representing input events, exposed to JavaScript as "events".
  // It contains a description of each input event that has happened since the
  // last time the "events" JavaScript property was accessed.
  std::string events_;
};

// EventInstance handles input events by passing them to an appropriate function
// on event_handler_.  See EventHandler more more information.
class EventInstance : public pp::Instance {
 public:
  explicit EventInstance(PP_Instance instance)
      : pp::Instance(instance),
        event_handler_(NULL) {
    std::printf("EventInstance created.\n");
  }
  virtual ~EventInstance() {}

  virtual pp::Var GetInstanceObject() {
    std::printf("EventInstance::GetInstanceObject\n");
    event_handler_ = new EventHandler(this->pp_instance());
    // pp::Var takes ownership of the new EventHandler object.
    return pp::Var(event_handler_);
  }

  // Handle an incoming input event by switching on type, converting the event
  // appropriately, and forwarding it on to event_handler_.
  virtual bool HandleInputEvent(const PP_InputEvent& event) {
    std::printf("HandleInputEvent called\n");
    if (NULL == event_handler_) {
      // It's possible that the EventHandler was not yet created when the input
      // event happened.  The EventHandler gets created when GetInstanceObject
      // is called, and that happens the first time the scripting interface is
      // used.  event_example.html forces this to happen early by accessing the
      // "events" property on startup, but it may be possible for input events
      // to happen and be passed to the plugin before that JavaScript executes.
      // We just do nothing until the EventHandler is created.
      std::printf("event_handler_ is null\n");
      return false;
    }
    switch (event.type) {
      case PP_INPUTEVENT_TYPE_UNDEFINED:
        std::printf("Undefined event.\n");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        event_handler_->MouseEvent(event.u.mouse, event.time_stamp, "Down");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEUP:
        event_handler_->MouseEvent(event.u.mouse, event.time_stamp, "Up");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
        event_handler_->MouseEvent(event.u.mouse, event.time_stamp, "Move");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEENTER:
        event_handler_->MouseEvent(event.u.mouse, event.time_stamp, "Enter");
        break;
      case PP_INPUTEVENT_TYPE_MOUSELEAVE:
        event_handler_->MouseEvent(event.u.mouse, event.time_stamp, "Leave");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEWHEEL:
        event_handler_->WheelEvent(event.u.wheel, event.time_stamp);
        break;
      case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
        event_handler_->KeyEvent(event.u.key, event.time_stamp, "RawKeyDown");
        break;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        event_handler_->KeyEvent(event.u.key, event.time_stamp, "Down");
        break;
      case PP_INPUTEVENT_TYPE_KEYUP:
        event_handler_->KeyEvent(event.u.key, event.time_stamp, "Up");
        break;
      case PP_INPUTEVENT_TYPE_CHAR:
        event_handler_->CharEvent(event.u.character, event.time_stamp);
        break;
      default:
        std::printf("Unrecognized event type: %d\n", event.type);
        assert(false);
        return false;
    }
    return true;
  }

 private:
  EventHandler* event_handler_;
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
