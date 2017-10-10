// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDisplayMode_h
#define WebDisplayMode_h

namespace blink {

// Enumerates values of the display property of the Web App Manifest.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.platform
// GENERATED_JAVA_PREFIX_TO_STRIP: WebDisplayMode
enum WebDisplayMode {
  kWebDisplayModeUndefined,  // User for override setting (ie. not set).
  kWebDisplayModeBrowser,
  kWebDisplayModeMinimalUi,
  kWebDisplayModeStandalone,
  kWebDisplayModeFullscreen,
  kWebDisplayModeLast = kWebDisplayModeFullscreen
  // This enum is persisted to logs, and therefore is append-only and should not
  // be reordered.
};

}  // namespace blink

#endif  // WebDisplayMode_h
