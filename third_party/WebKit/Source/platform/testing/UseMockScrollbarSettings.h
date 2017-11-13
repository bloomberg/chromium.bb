// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UseMockScrollbarSettings_h
#define UseMockScrollbarSettings_h

#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

// Forces to use mocked overlay scrollbars instead of the default native theme
// scrollbars to avoid crash in Chromium code when it tries to load UI
// resources that are not available when running blink unit tests, and to
// ensure consistent layout regardless of differences between scrollbar themes.
// WebViewHelper includes this, so this is only needed if a test doesn't use
// WebViewHelper or the test needs a bigger scope of mock scrollbar settings
// than the scope of WebViewHelper.
class UseMockScrollbarSettings : private ScopedOverlayScrollbarsForTest {
 public:
  UseMockScrollbarSettings()
      : ScopedOverlayScrollbarsForTest(true),
        original_mock_scrollbar_enabled_(
            ScrollbarTheme::MockScrollbarsEnabled()),
        original_overlay_scrollbars_enabled_(
            RuntimeEnabledFeatures::OverlayScrollbarsEnabled()) {
    ScrollbarTheme::SetMockScrollbarsEnabled(true);
  }

  UseMockScrollbarSettings(bool use_mock, bool use_overlay)
      : ScopedOverlayScrollbarsForTest(use_overlay),
        original_mock_scrollbar_enabled_(
            ScrollbarTheme::MockScrollbarsEnabled()),
        original_overlay_scrollbars_enabled_(
            RuntimeEnabledFeatures::OverlayScrollbarsEnabled()) {
    ScrollbarTheme::SetMockScrollbarsEnabled(use_mock);
  }

  ~UseMockScrollbarSettings() {
    ScrollbarTheme::SetMockScrollbarsEnabled(original_mock_scrollbar_enabled_);
  }

 private:
  bool original_mock_scrollbar_enabled_;
  bool original_overlay_scrollbars_enabled_;
};

}  // namespace blink

#endif  // UseMockScrollbarSettings_h
