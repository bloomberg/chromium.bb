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
  kWebClientHintsTypeDeviceRam,
  kWebClientHintsTypeDpr,
  kWebClientHintsTypeResourceWidth,
  kWebClientHintsTypeViewportWidth,

  // Last client hint type.
  kWebClientHintsTypeLast = kWebClientHintsTypeViewportWidth
};

}  // namespace blink

#endif  // WebClientHintsType_h
