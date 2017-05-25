// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScreenOrientationType_h
#define WebScreenOrientationType_h

namespace blink {

enum WebScreenOrientationType {
  kWebScreenOrientationUndefined = 0,
  kWebScreenOrientationPortraitPrimary,
  kWebScreenOrientationPortraitSecondary,
  kWebScreenOrientationLandscapePrimary,
  kWebScreenOrientationLandscapeSecondary,

  WebScreenOrientationTypeLast = kWebScreenOrientationLandscapeSecondary
};

}  // namespace blink

#endif  // WebScreenOrientationType_h
