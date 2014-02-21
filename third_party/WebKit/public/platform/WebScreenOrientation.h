// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScreenOrientation_h
#define WebScreenOrientation_h

namespace blink {

enum WebScreenOrientation {
    WebScreenOrientationPortraitPrimary = 1,
    WebScreenOrientationLandscapePrimary = 1 << 1,
    WebScreenOrientationPortraitSecondary = 1 << 2,
    WebScreenOrientationLandscapeSecondary = 1 << 3,
};

typedef unsigned char WebScreenOrientations;

} // namespace blink

#endif // WebScreenOrientation_h
