// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameOwnerProperties_h
#define WebFrameOwnerProperties_h

namespace blink {

struct WebFrameOwnerProperties {
    enum class ScrollingMode {
        Auto,
        AlwaysOff,
        AlwaysOn,
        Last = AlwaysOn
    };

    ScrollingMode scrollingMode;
    int marginWidth;
    int marginHeight;

    WebFrameOwnerProperties()
        : scrollingMode(ScrollingMode::Auto)
        , marginWidth(-1)
        , marginHeight(-1)
    {
    }

#if INSIDE_BLINK
    WebFrameOwnerProperties(ScrollbarMode scrollingMode, int marginWidth, int marginHeight)
        : scrollingMode(static_cast<ScrollingMode>(scrollingMode))
        , marginWidth(marginWidth)
        , marginHeight(marginHeight)
    {
    }
#endif
};

} // namespace blink

#endif
