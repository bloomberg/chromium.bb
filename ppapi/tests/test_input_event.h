// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_INPUT_EVENT_H_
#define PPAPI_TESTS_TEST_INPUT_EVENT_H_

#include <string>
#include <vector>

#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/test_case.h"

struct PPB_InputEvent;
struct PPB_MouseInputEvent;
struct PPB_WheelInputEvent;
struct PPB_KeyboardInputEvent;
struct PPB_Testing_Dev;

class TestInputEvent : public TestCase {
 public:
  explicit TestInputEvent(TestingInstance* instance);
  ~TestInputEvent();

  bool HandleInputEvent(const pp::InputEvent& input_event);
  void HandleMessage(const pp::Var& message_data);
  void DidChangeView(const pp::Rect& position, const pp::Rect& clip);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& test_filter);

 private:
  pp::InputEvent CreateMouseEvent(PP_InputEvent_Type type,
                                  PP_InputEvent_MouseButton buttons);
  pp::InputEvent CreateWheelEvent();
  pp::InputEvent CreateKeyEvent(PP_InputEvent_Type type,
                                uint32_t key_code);
  pp::InputEvent CreateCharEvent(const std::string& text);

  bool SimulateInputEvent(const pp::InputEvent& input_event);
  bool AreEquivalentEvents(PP_Resource first, PP_Resource second);

  std::string TestEvents();

  const struct PPB_InputEvent* input_event_interface_;
  const struct PPB_MouseInputEvent* mouse_input_event_interface_;
  const struct PPB_WheelInputEvent* wheel_input_event_interface_;
  const struct PPB_KeyboardInputEvent* keyboard_input_event_interface_;

  pp::Rect view_rect_;
  pp::InputEvent expected_input_event_;
  bool received_expected_event_;
  bool received_finish_message_;
};

#endif  // PPAPI_TESTS_TEST_INPUT_EVENT_H_

