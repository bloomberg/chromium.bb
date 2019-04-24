// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COLOR_SCHEME_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COLOR_SCHEME_H_

namespace blink {

// Use for passing preferred color scheme from the OS to the renderer.
enum class WebColorScheme {
  kNoPreference,
  kDark,
  kLight,
};

}  // namespace blink

#endif
