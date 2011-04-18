// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <sstream>
#include <string>
#include <queue>

#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

using std::string;
using std::queue;
using std::ostringstream;

const int kDefaultEventBufferSize = 10;

namespace {

string ModifierToString(uint32_t modifier) {
  string s;
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

string MouseButtonToString(PP_InputEvent_MouseButton button) {
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
    ostringstream stream;
    stream << "Unrecognized (" << static_cast<int32_t>(button) << ")";
    return stream.str();
  }
}

string KeyEvent(PP_InputEvent_Key key,
                PP_TimeTicks time,
                const string& kind) {
  ostringstream stream;
  stream << "Key event:" << kind.c_str()
         << " modifier:" << ModifierToString(key.modifier)
         << " key_code:" << key.key_code
         << " time:" << time
         << "\n";
  return stream.str();
}

string MouseEvent(PP_InputEvent_Mouse mouse_event,
                PP_TimeTicks time,
                const string& kind) {
  ostringstream stream;
  stream << "Mouse event:" << kind.c_str()
         << " modifier:" << ModifierToString(mouse_event.modifier).c_str()
         << " button:" << MouseButtonToString(mouse_event.button).c_str()
         << " x:" << mouse_event.x
         << " y:" << mouse_event.y
         << " click_count:" << mouse_event.click_count
         << " time:" << time
         << "\n";
  return stream.str();
}

string CharEvent(PP_InputEvent_Character char_event, PP_TimeTicks time) {
  ostringstream stream;
  stream << "Character event."
         << " modifier:" << ModifierToString(char_event.modifier).c_str()
         << " text:" << char_event.text
         << " time:" << time
         << "\n";
  return stream.str();
}


string WheelEvent(PP_InputEvent_Wheel wheel_event, PP_TimeTicks time) {
  ostringstream stream;
  stream << "Wheel event."
         << " modifier:" << ModifierToString(wheel_event.modifier).c_str()
         << " deltax:" << wheel_event.delta_x
         << " deltay:" << wheel_event.delta_y
         << " wheel_ticks_x:" << wheel_event.wheel_ticks_x
         << " wheel_ticks_y:" << wheel_event.wheel_ticks_y
         << " scroll_by_page:"
         << (wheel_event.scroll_by_page ? "true" : "false")
         << "\n";
  return stream.str();
}

string EventToString(const PP_InputEvent& event) {
  ostringstream stream;
  switch (event.type) {
   default:
   case PP_INPUTEVENT_TYPE_UNDEFINED:
    stream << "Unrecognized Event (" << static_cast<int32_t>(event.type) << ")";
    return stream.str();

   case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    return MouseEvent(event.u.mouse, event.time_stamp, "Down");
   case PP_INPUTEVENT_TYPE_MOUSEUP:
    return MouseEvent(event.u.mouse, event.time_stamp, "Up");
   case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    return MouseEvent(event.u.mouse, event.time_stamp, "Move");
   case PP_INPUTEVENT_TYPE_MOUSEENTER:
    return MouseEvent(event.u.mouse, event.time_stamp, "Enter");
   case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    return MouseEvent(event.u.mouse, event.time_stamp, "Leave");
   case PP_INPUTEVENT_TYPE_MOUSEWHEEL:
    return WheelEvent(event.u.wheel, event.time_stamp);
   case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    return KeyEvent(event.u.key, event.time_stamp, "RawKeyDown");
   case PP_INPUTEVENT_TYPE_KEYDOWN:
    return KeyEvent(event.u.key, event.time_stamp, "Down");
   case PP_INPUTEVENT_TYPE_KEYUP:
    return KeyEvent(event.u.key, event.time_stamp, "Up");
   case PP_INPUTEVENT_TYPE_CHAR:
    return CharEvent(event.u.character, event.time_stamp);
  }
}

} // namespace


class MyInstance : public pp::Instance {
 private:
  size_t max_buffer_size_;
  queue<PP_InputEvent> event_buffer_;

  void ParseArgs(uint32_t argc, const char* argn[], const char* argv[]) {
     for (uint32_t i = 0; i < argc; ++i) {
       const std::string tag = argn[i];
       if (tag == "buffer_size") max_buffer_size_ = strtol(argv[i], 0, 0);
       // ignore other tags
     }
  }

  void FlushEventBuffer() {
      ostringstream stream;
      while (event_buffer_.size() > 0) {
        stream << pp_instance() << ": " << EventToString(event_buffer_.front());
        event_buffer_.pop();
      }
      pp::Var message(stream.str());
      PostMessage(message);
  }

 public:
  explicit MyInstance(PP_Instance instance)
    : pp::Instance(instance), max_buffer_size_(kDefaultEventBufferSize) {}

  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    ParseArgs(argc, argn, argv);
    return true;
  }

  virtual bool HandleInputEvent(const PP_InputEvent& event) {
    event_buffer_.push(event);
    if (event_buffer_.size() >= max_buffer_size_) {
      FlushEventBuffer();
    }
    return true;
  }

  virtual void DidChangeFocus(bool has_focus) {
    FlushEventBuffer();
  }

};

// standard boilerplate code below
class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {
  Module* CreateModule() {
    return new MyModule();
  }
}
