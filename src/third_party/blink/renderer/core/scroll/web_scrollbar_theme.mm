/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"

#import <AppKit/AppKit.h>

#include "third_party/blink/renderer/core/scroll/ns_scroller_imp_details.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_mac.h"

namespace blink {

static_assert(static_cast<NSScrollerStyle>(kScrollerStyleLegacy) ==
                  NSScrollerStyleLegacy,
              "ScrollerStyleLegacy must match NSScrollerStyleLegacy");
static_assert(static_cast<NSScrollerStyle>(kScrollerStyleOverlay) ==
                  NSScrollerStyleOverlay,
              "ScrollerStyleOverlay must match NSScrollerStyleOverlay");

typedef WTF::HashSet<WebScrollbarThemeClient*> ClientSet;

static ClientSet& GetClientSet() {
  DEFINE_STATIC_LOCAL(ClientSet, set, ());
  return set;
}

static float s_initial_button_delay = 0.5f;
static float s_autoscroll_button_delay = 0.05f;
static ScrollerStyle s_preferred_scroller_style = kScrollerStyleLegacy;
static bool s_jump_on_track_click = false;

void WebScrollbarTheme::UpdateScrollbarsWithNSDefaults(
    float initial_button_delay,
    float autoscroll_button_delay,
    ScrollerStyle preferred_scroller_style,
    bool redraw,
    bool jump_on_track_click) {
  s_initial_button_delay = initial_button_delay;
  s_autoscroll_button_delay = autoscroll_button_delay;
  s_preferred_scroller_style = preferred_scroller_style;
  s_jump_on_track_click = jump_on_track_click;

  if (redraw && !GetClientSet().IsEmpty()) {
    for (auto* client : GetClientSet()) {
      client->PreferencesChanged();
    }
  }
}

float WebScrollbarTheme::InitialButtonDelay() {
  return s_initial_button_delay;
}

float WebScrollbarTheme::AutoscrollButtonDelay() {
  return s_autoscroll_button_delay;
}

ScrollerStyle WebScrollbarTheme::PreferredScrollerStyle() {
  return s_preferred_scroller_style;
}

bool WebScrollbarTheme::JumpOnTrackClick() {
  return s_jump_on_track_click;
}

void WebScrollbarTheme::RegisterClient(WebScrollbarThemeClient& client) {
  GetClientSet().insert(&client);
}

void WebScrollbarTheme::UnregisterClient(WebScrollbarThemeClient& client) {
  GetClientSet().erase(&client);
}

}  // namespace blink
