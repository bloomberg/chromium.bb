/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "build/build_config.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/page/create_window.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

NavigationPolicy NavigationPolicyFromMouseEvent(unsigned short button,
                                                bool ctrl,
                                                bool shift,
                                                bool alt,
                                                bool meta) {
#if defined(OS_MACOSX)
  const bool new_tab_modifier = (button == 1) || meta;
#else
  const bool new_tab_modifier = (button == 1) || ctrl;
#endif
  if (!new_tab_modifier && !shift && !alt)
    return kNavigationPolicyCurrentTab;

  if (new_tab_modifier) {
    if (shift)
      return kNavigationPolicyNewForegroundTab;
    else
      return kNavigationPolicyNewBackgroundTab;
  } else {
    if (shift)
      return kNavigationPolicyNewWindow;
    else
      return kNavigationPolicyDownload;
  }
  return kNavigationPolicyCurrentTab;
}

namespace {

NavigationPolicy NavigationPolicyFromEventInternal(Event* event) {
  if (!event)
    return kNavigationPolicyCurrentTab;

  if (event->IsMouseEvent()) {
    MouseEvent* mouse_event = ToMouseEvent(event);
    return NavigationPolicyFromMouseEvent(
        mouse_event->button(), mouse_event->ctrlKey(), mouse_event->shiftKey(),
        mouse_event->altKey(), mouse_event->metaKey());
  } else if (event->IsKeyboardEvent()) {
    // The click is simulated when triggering the keypress event.
    KeyboardEvent* key_event = ToKeyboardEvent(event);
    return NavigationPolicyFromMouseEvent(
        0, key_event->ctrlKey(), key_event->shiftKey(), key_event->altKey(),
        key_event->metaKey());
  } else if (event->IsGestureEvent()) {
    // The click is simulated when triggering the gesture-tap event
    GestureEvent* gesture_event = ToGestureEvent(event);
    return NavigationPolicyFromMouseEvent(
        0, gesture_event->ctrlKey(), gesture_event->shiftKey(),
        gesture_event->altKey(), gesture_event->metaKey());
  }
  return kNavigationPolicyCurrentTab;
}

}  // namespace

NavigationPolicy NavigationPolicyFromEvent(Event* event) {
  NavigationPolicy policy = NavigationPolicyFromEventInternal(event);
  // TODO(dgozman): move navigation policy helpers from CreateWindow here.
  if (policy == kNavigationPolicyDownload &&
      EffectiveNavigationPolicy(policy, CurrentInputEvent::Get(),
                                WebWindowFeatures()) !=
          kNavigationPolicyDownload) {
    return kNavigationPolicyCurrentTab;
  }
  return policy;
}

STATIC_ASSERT_ENUM(kWebNavigationPolicyIgnore, kNavigationPolicyIgnore);
STATIC_ASSERT_ENUM(kWebNavigationPolicyDownload, kNavigationPolicyDownload);
STATIC_ASSERT_ENUM(kWebNavigationPolicyCurrentTab, kNavigationPolicyCurrentTab);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewBackgroundTab,
                   kNavigationPolicyNewBackgroundTab);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewForegroundTab,
                   kNavigationPolicyNewForegroundTab);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewWindow, kNavigationPolicyNewWindow);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewPopup, kNavigationPolicyNewPopup);

}  // namespace blink
