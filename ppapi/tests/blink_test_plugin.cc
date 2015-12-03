// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <utility>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

namespace {

void DummyCompletionCallback(void*, int32_t) {}

// This is a simple C++ Pepper plugin for Blink layout tests.
//
// Layout tests can instantiate this plugin by requesting the mime type
// application/x-blink-test-plugin. When possible, tests should use the
// startAfterLoadAndFinish() helper in resources/plugin.js to perform work
// after the plugin has loaded.
//
// The plugin also exposes several other features for testing convenience:
// - On first paint, the plugin posts a 'loaded' message to its owner element.
// - On subsequent paints, the plugin posts a 'painted' message instead.
// - Keyboard and mouse input events are logged to the console.
class BlinkTestInstance : public pp::Instance {
 public:
  explicit BlinkTestInstance(PP_Instance instance)
      : pp::Instance(instance), first_paint_(true) {}
  ~BlinkTestInstance() override {}

  bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                                       PP_INPUTEVENT_CLASS_KEYBOARD) == PP_OK;
  }

  void DidChangeView(const pp::View& view) override {
    view_ = view;
    device_context_ = pp::Graphics2D(this, view_.GetRect().size(), true);
    if (!BindGraphics(device_context_))
      return;

    // Since we draw a static image, we only need to make a new frame when
    // the device is initialized or the view size changes.
    Paint();
  }

  void DidChangeFocus(bool has_focus) override {
    LogMessage("DidChangeFocus(", has_focus, ")");
  }

  bool HandleInputEvent(const pp::InputEvent& event) override {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        LogMouseEvent("Down", event);
        break;
      case PP_INPUTEVENT_TYPE_MOUSEUP:
        LogMouseEvent("Up", event);
        break;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        LogKeyboardEvent("Down", event);
        break;
      case PP_INPUTEVENT_TYPE_KEYUP:
        LogKeyboardEvent("Up", event);
        break;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      case PP_INPUTEVENT_TYPE_MOUSEENTER:
      case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      case PP_INPUTEVENT_TYPE_CHAR:
        // Just swallow these events without any logging.
        return true;
      default:
        LogMessage("Unexpected input event with type = ", event.GetType());
        return false;
    }
    return true;
  }

 private:
  void Paint() {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                        view_.GetRect().size(), true);
    if (image.is_null())
      return;

    // Draw blue and green checkerboard pattern to show "interesting" keyframe.
    const int kSquareSizePixels = 8;
    for (int y = 0; y < view_.GetRect().size().height(); ++y) {
      for (int x = 0; x < view_.GetRect().size().width(); ++x) {
        int x_square = x / kSquareSizePixels;
        int y_square = y / kSquareSizePixels;
        uint32_t color = ((x_square + y_square) % 2) ? 0xFF0000FF : 0xFF00FF00;
        *image.GetAddr32(pp::Point(x, y)) = color;
      }
    }

    device_context_.ReplaceContents(&image);
    device_context_.Flush(
        pp::CompletionCallback(&DummyCompletionCallback, nullptr));

    // TODO(dcheng): In theory, this should wait for the flush to complete
    // before claiming that it's painted. In practice, this is difficult: when
    // running layout tests, a frame is typically only generated at the end of
    // the layout test. Sending the completion message in the callback results
    // in a deadlock: the test wants to wait for the plugin to paint, but the
    // plugin won't paint until the test completes. This seems to be Good
    // Enough™ for now.
    if (first_paint_) {
      first_paint_ = false;
      PostMessage(pp::Var("loaded"));
    } else {
      PostMessage(pp::Var("painted"));
    }
  }

  void LogMouseEvent(const std::string& type, const pp::InputEvent& event) {
    pp::MouseInputEvent mouse_event(event);
    pp::Point mouse_position = mouse_event.GetPosition();
    LogMessage("Mouse", type, " at (", mouse_position.x(), ",",
               mouse_position.y(), ")");
  }

  void LogKeyboardEvent(const std::string& type, const pp::InputEvent& event) {
    pp::KeyboardInputEvent keyboard_event(event);
    LogMessage("Key", type, " '", keyboard_event.GetCode().AsString(), "'");
  }

  // Template magic to cover the lack of base::StringPrintf.
  template <typename... Args>
  void LogMessage(const Args&... args) {
    std::ostringstream ss;
    ss << std::boolalpha;
    LogMessageHelper(&ss, args...);
  }

  template <typename Arg, typename... Args>
  void LogMessageHelper(std::ostringstream* os,
                        const Arg& arg,
                        const Args&... args) {
    *os << arg;
    LogMessageHelper(os, args...);
  }

  template <typename Arg>
  void LogMessageHelper(std::ostringstream* os, const Arg& arg) {
    *os << arg;
    LogToConsoleWithSource(PP_LOGLEVEL_LOG, pp::Var("Blink Test Plugin"),
                           pp::Var(os->str()));
  }

  bool first_paint_;
  pp::View view_;
  pp::Graphics2D device_context_;
};

class BlinkTestModule : public pp::Module {
 public:
  BlinkTestModule() {}
  virtual ~BlinkTestModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new BlinkTestInstance(instance);
  }
};

}  // namespace

namespace pp {

Module* CreateModule() {
  return new BlinkTestModule();
}

}  // namespace pp
