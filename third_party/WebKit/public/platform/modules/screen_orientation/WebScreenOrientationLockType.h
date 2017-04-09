// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScreenOrientationLockType_h
#define WebScreenOrientationLockType_h

namespace blink {

enum WebScreenOrientationLockType {
  kWebScreenOrientationLockDefault = 0,  // Equivalent to unlock.
  kWebScreenOrientationLockPortraitPrimary,
  kWebScreenOrientationLockPortraitSecondary,
  kWebScreenOrientationLockLandscapePrimary,
  kWebScreenOrientationLockLandscapeSecondary,
  kWebScreenOrientationLockAny,
  kWebScreenOrientationLockLandscape,
  kWebScreenOrientationLockPortrait,
  kWebScreenOrientationLockNatural,
};

}  // namespace blink

#endif  // WebScreenOrientationLockType_h
