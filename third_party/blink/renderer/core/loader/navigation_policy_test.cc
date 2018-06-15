/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "base/auto_reset.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"

namespace blink {

// TODO(dgozman): add tests for NavigationPolicyFromEvent.
class NavigationPolicyTest : public testing::Test {
 protected:
  NavigationPolicy GetPolicyForCreateWindow(int modifiers,
                                            WebMouseEvent::Button button,
                                            bool as_popup) {
    WebMouseEvent event(WebInputEvent::kMouseUp, modifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.button = button;
    if (as_popup)
      features.tool_bar_visible = false;
    base::AutoReset<const WebInputEvent*> current_event_change(
        &CurrentInputEvent::current_input_event_, &event);
    return NavigationPolicyForCreateWindow(features);
  }

  WebWindowFeatures features;
};

TEST_F(NavigationPolicyTest, LeftClick) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, LeftClickPopup) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ShiftLeftClick) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewWindow,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ShiftLeftClickPopup) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaLeftClick) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaLeftClickPopup) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaAndShiftLeftClick) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  modifiers |= WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaAndShiftLeftClickPopup) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  modifiers |= WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, MiddleClick) {
  int modifiers = 0;
  bool as_popup = false;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, MiddleClickPopup) {
  int modifiers = 0;
  bool as_popup = true;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, NoToolbarsForcesPopup) {
  features.tool_bar_visible = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.tool_bar_visible = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, NoStatusBarForcesPopup) {
  features.status_bar_visible = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.status_bar_visible = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, NoMenuBarForcesPopup) {
  features.menu_bar_visible = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.menu_bar_visible = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, NotResizableForcesPopup) {
  features.resizable = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.resizable = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

}  // namespace blink
