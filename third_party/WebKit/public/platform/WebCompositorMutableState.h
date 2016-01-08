// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorMutableState_h
#define WebCompositorMutableState_h

#include <cstdint>

class SkMatrix44;

namespace blink {

// This class wraps the compositor-owned, mutable state for a single element.
class WebCompositorMutableState {
public:
    virtual ~WebCompositorMutableState() { }

    virtual double opacity() const = 0;
    virtual void setOpacity(double) = 0;

    virtual const SkMatrix44& transform() const = 0;
    virtual void setTransform(const SkMatrix44&) = 0;

    virtual double scrollLeft() const = 0;
    virtual void setScrollLeft(double) = 0;

    virtual double scrollTop() const = 0;
    virtual void setScrollTop(double) = 0;
};

} // namespace blink

#endif // WebCompositorMutableState_h
