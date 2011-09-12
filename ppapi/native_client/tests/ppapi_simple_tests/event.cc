// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <sstream>
#include <string>
#include <queue>

#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/input_event.h"


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


string KeyEvent(const pp::KeyboardInputEvent& key_event,
                const string& kind) {
  ostringstream stream;
  stream << "Key event:" << kind
         << " modifier:" << ModifierToString(key_event.GetModifiers())
         << " key_code:" << key_event.GetKeyCode()
         << " time:" << key_event.GetTimeStamp()
         << " text:" << key_event.GetCharacterText().DebugString()
         << "\n";
  return stream.str();
}


string MouseEvent(const pp::MouseInputEvent& mouse_event,
                  const string& kind) {
  ostringstream stream;
  stream << "Mouse event:" << kind
         << " modifier:" << ModifierToString(mouse_event.GetModifiers())
         << " button:" << MouseButtonToString(mouse_event.GetButton())
         << " x:" << mouse_event.GetPosition().x()
         << " y:" << mouse_event.GetPosition().y()
         << " click_count:" << mouse_event.GetClickCount()
         << " time:" << mouse_event.GetTimeStamp()
         << "\n";
  return stream.str();
}


string WheelEvent(const pp::WheelInputEvent& wheel_event) {
  ostringstream stream;
  stream << "Wheel event."
         << " modifier:" << ModifierToString(wheel_event.GetModifiers())
         << " deltax:" << wheel_event.GetDelta().x()
         << " deltay:" << wheel_event.GetDelta().y()
         << " wheel_ticks_x:" << wheel_event.GetTicks().x()
         << " wheel_ticks_y:" << wheel_event.GetTicks().y()
         << " scroll_by_page:"
         << (wheel_event.GetScrollByPage() ? "true" : "false")
         << "\n";
  return stream.str();
}


string EventToString(const pp::InputEvent& event) {
  ostringstream stream;
  switch (event.GetType()) {
    default:
    case PP_INPUTEVENT_TYPE_UNDEFINED:
     stream << "Unrecognized Event (" << static_cast<int32_t>(event.GetType())
             << ")";
      return stream.str();

    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      return MouseEvent(pp::MouseInputEvent(event), "Down");
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      return MouseEvent(pp::MouseInputEvent(event), "Up");
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      return MouseEvent(pp::MouseInputEvent(event), "Move");
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      return MouseEvent(pp::MouseInputEvent(event), "Enter");
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      return MouseEvent(pp::MouseInputEvent(event), "Leave");

    case PP_INPUTEVENT_TYPE_WHEEL:
      return WheelEvent(pp::WheelInputEvent(event));

    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      return KeyEvent(pp::KeyboardInputEvent(event), "RawKeyDown");
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      return KeyEvent(pp::KeyboardInputEvent(event), "Down");
    case PP_INPUTEVENT_TYPE_KEYUP:
      return KeyEvent(pp::KeyboardInputEvent(event), "Up");
    case PP_INPUTEVENT_TYPE_CHAR:
      return KeyEvent(pp::KeyboardInputEvent(event), "Char");
  }
}

void StringReplace(string* input,
                   const string& find,
                   const string& replace) {
  if (find.length() == 0 || input->length() == 0) {
    return;
  }

  size_t start_pos = 0;
  while (1) {
    start_pos = input->find(find, start_pos);
    if (start_pos == string::npos) {
      break;
    }
    input->replace(start_pos, find.length(), replace);
    start_pos += replace.length();
  }
}

}  // namespace


class MyInstance : public pp::Instance {
 private:
  size_t max_buffer_size_;
  queue<string> event_buffer_;

  void ParseArgs(uint32_t argc, const char* argn[], const char* argv[]) {
     for (uint32_t i = 0; i < argc; ++i) {
       const std::string tag = argn[i];
       if (tag == "buffer_size") max_buffer_size_ = strtol(argv[i], 0, 0);
       // ignore other tags
     }
  }

  // Dump all the event via PostMessage for testing
  void FlushEventBuffer() {
      while (event_buffer_.size() > 0) {
        string s = event_buffer_.front();
        event_buffer_.pop();
        // Replace space with underscore to simplify testing
        StringReplace(&s, " ", "_");
        pp::Var message(s);
        PostMessage(message);
      }
  }

 public:
  explicit MyInstance(PP_Instance instance)
    : pp::Instance(instance), max_buffer_size_(kDefaultEventBufferSize) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_WHEEL);
    RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
  }

  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    ParseArgs(argc, argn, argv);
    return true;
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
     ostringstream stream;
     stream << pp_instance() << ": " << EventToString(event);
     event_buffer_.push(stream.str());
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
