// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_MANAGER_H_

#include <text-input-unstable-v1-server-protocol.h>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct zwp_text_input_manager_v1_interface
    kTestZwpTextInputManagerV1Impl;

class MockZwpTextInput;

// Manage zwp_text_input_manager_v1 object.
class TestZwpTextInputManagerV1 : public GlobalObject {
 public:
  TestZwpTextInputManagerV1();
  ~TestZwpTextInputManagerV1() override;

  void set_text_input(MockZwpTextInput* text_input) {
    text_input_ = text_input;
  }
  MockZwpTextInput* text_input() const { return text_input_; }

 private:
  MockZwpTextInput* text_input_;

  DISALLOW_COPY_AND_ASSIGN(TestZwpTextInputManagerV1);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_MANAGER_H_
