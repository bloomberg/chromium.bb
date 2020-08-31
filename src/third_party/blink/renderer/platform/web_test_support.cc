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

#include "third_party/blink/renderer/platform/web_test_support.h"

#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// ==== Functions defined in third_party/blink/public/web/blink.h. ====

void SetWebTestMode(bool value) {
  WebTestSupport::SetIsRunningWebTest(value);
}

bool WebTestMode() {
  return WebTestSupport::IsRunningWebTest();
}

void SetFontAntialiasingEnabledForTest(bool value) {
  WebTestSupport::SetFontAntialiasingEnabledForTest(value);
}

bool FontAntialiasingEnabledForTest() {
  return WebTestSupport::IsFontAntialiasingEnabledForTest();
}

// ==== State methods defined in WebTestSupport. ====

static bool g_is_running_web_test = false;
static bool g_is_font_antialiasing_enabled = false;
static bool g_is_subpixel_positioning_allowed = true;

bool WebTestSupport::IsRunningWebTest() {
  return g_is_running_web_test;
}

void WebTestSupport::SetIsRunningWebTest(bool value) {
  g_is_running_web_test = value;
}

bool WebTestSupport::IsFontAntialiasingEnabledForTest() {
  return g_is_font_antialiasing_enabled;
}

void WebTestSupport::SetFontAntialiasingEnabledForTest(bool value) {
  g_is_font_antialiasing_enabled = value;
}

bool WebTestSupport::IsTextSubpixelPositioningAllowedForTest() {
  return g_is_subpixel_positioning_allowed;
}

void WebTestSupport::SetTextSubpixelPositioningAllowedForTest(bool value) {
  g_is_subpixel_positioning_allowed = value;
}

}  // namespace blink
