// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformGestureCurveTarget_h
#define PlatformGestureCurveTarget_h

namespace WebCore {

class IntPoint;

class PlatformGestureCurveTarget {
public:
    virtual void scrollBy(const IntPoint&) = 0;
    // FIXME: add interfaces for scroll(), etc.

protected:
    virtual ~PlatformGestureCurveTarget() { }
};

} // namespace WebCore

#endif
