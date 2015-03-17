// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeViewportAnchor_h
#define ResizeViewportAnchor_h

#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "web/ViewportAnchor.h"

namespace blink {

class FrameView;
class PinchViewport;

// The resize anchor saves the current scroll offset of the visual viewport and
// restores to that scroll offset so that document location appears exactly
// unchanged to the user.
class ResizeViewportAnchor : public ViewportAnchor {
public:
    ResizeViewportAnchor(FrameView& rootFrameView, PinchViewport&);
    virtual ~ResizeViewportAnchor() { }

    virtual void setAnchor();
    virtual void restoreToAnchor();

private:
    // Inner viewport origin in the reference frame of the root document, in CSS
    // pixels.
    FloatPoint m_pinchViewportInDocument;
};

} // namespace blink

#endif
