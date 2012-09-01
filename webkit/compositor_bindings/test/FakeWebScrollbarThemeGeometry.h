// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeWebScrollbarThemeGeometry_h
#define FakeWebScrollbarThemeGeometry_h

#include <public/WebScrollbarThemeGeometry.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class FakeWebScrollbarThemeGeometry : public WebKit::WebScrollbarThemeGeometry {
public:
    static PassOwnPtr<WebKit::WebScrollbarThemeGeometry> create() { return adoptPtr(new WebKit::FakeWebScrollbarThemeGeometry()); }

    virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE
    {
        return new FakeWebScrollbarThemeGeometry();
    }

    virtual int thumbPosition(WebScrollbar*) OVERRIDE { return 0; }
    virtual int thumbLength(WebScrollbar*) OVERRIDE { return 0; }
    virtual int trackPosition(WebScrollbar*) OVERRIDE { return 0; }
    virtual int trackLength(WebScrollbar*) OVERRIDE { return 0; }
    virtual bool hasButtons(WebScrollbar*) OVERRIDE { return false; }
    virtual bool hasThumb(WebScrollbar*) OVERRIDE { return false; }
    virtual WebRect trackRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect thumbRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual int minimumThumbLength(WebScrollbar*) OVERRIDE { return 0; }
    virtual int scrollbarThickness(WebScrollbar*) OVERRIDE { return 0; }
    virtual WebRect backButtonStartRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect backButtonEndRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect forwardButtonStartRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect forwardButtonEndRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect constrainTrackRectToTrackPieces(WebScrollbar*, const WebRect&) OVERRIDE { return WebRect(); }

    virtual void splitTrack(WebScrollbar*, const WebRect& track, WebRect& startTrack, WebRect& thumb, WebRect& endTrack) OVERRIDE
    {
        startTrack = WebRect();
        thumb = WebRect();
        endTrack = WebRect();
    }
};

} // namespace WebKit

#endif // FakeWebScrollbarThemeGeometry_h
