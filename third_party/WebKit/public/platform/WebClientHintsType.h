// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebClientHintsType_h
#define WebClientHintsType_h

namespace blink {

enum WebClientHintsType {
  // The order of the enums or the values must not be changed. New values should
  // only be added after the last value, and kWebClientHintsTypeLast should be
  // updated accordingly.
  kWebClientHintsTypeDeviceMemory,
  kWebClientHintsTypeDpr,
  kWebClientHintsTypeResourceWidth,
  kWebClientHintsTypeViewportWidth,

  // Last client hint type.
  kWebClientHintsTypeLast = kWebClientHintsTypeViewportWidth
};

// WebEnabledClientHints stores all the client hints along with whether the hint
// is enabled or not.
struct WebEnabledClientHints {
  WebEnabledClientHints() {}

  bool IsEnabled(WebClientHintsType type) const { return enabled_types_[type]; }
  void SetIsEnabled(WebClientHintsType type, bool should_send) {
    enabled_types_[type] = should_send;
  }

  bool enabled_types_[kWebClientHintsTypeLast + 1] = {};
};

}  // namespace blink

#endif  // WebClientHintsType_h
