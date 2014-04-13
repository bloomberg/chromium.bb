// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScreenOrientationListener_h
#define WebScreenOrientationListener_h

#include "WebScreenOrientationType.h"

namespace blink {

class WebScreenOrientationListener {
public:
    virtual ~WebScreenOrientationListener() { }

    // This method is called every time the screen orientation changes.
    virtual void didChangeScreenOrientation(WebScreenOrientationType) = 0;
};

} // namespace blink

#endif // WebScreenOrientationListener_h
