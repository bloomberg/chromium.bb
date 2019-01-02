// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCROLL_WEB_SCROLLBAR_THEME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCROLL_WEB_SCROLLBAR_THEME_CLIENT_H_

namespace blink {

class WebScrollbarThemeClient {
 protected:
  WebScrollbarThemeClient() = default;

 public:
  virtual void PreferencesChanged() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCROLL_WEB_SCROLLBAR_THEME_CLIENT_H_
