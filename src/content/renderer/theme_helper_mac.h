// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_THEME_HELPER_MAC_H_
#define CONTENT_RENDERER_THEME_HELPER_MAC_H_

#include <string>

namespace content {

// Updates the process-wide preferences for system theme colors, by setting
// the respective NSUserDefaults and posting notifications.
void SystemColorsDidChange(int aqua_color_variant,
                           const std::string& highlight_text_color,
                           const std::string& highlight_color);

}  // namespace content

#endif  // CONTENT_RENDERER_THEME_HELPER_MAC_H_
