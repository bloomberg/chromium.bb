// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameWidgetBase_h
#define WebFrameWidgetBase_h

#include "public/web/WebFrameWidget.h"
#include "wtf/Assertions.h"

namespace blink {

class CompositorAnimationTimeline;
class CompositorProxyClient;
class GraphicsLayer;
class WebLayer;

class WebFrameWidgetBase : public WebFrameWidget {
public:
    virtual bool forSubframe() const = 0;
    virtual void scheduleAnimation() = 0;
    virtual CompositorProxyClient* createCompositorProxyClient() = 0;
    virtual WebWidgetClient* client() const = 0;

    // Sets the root graphics layer. |GraphicsLayer| can be null when detaching
    // the root layer.
    virtual void setRootGraphicsLayer(GraphicsLayer*) = 0;

    // Sets the root layer. |WebLayer| can be null when detaching the root layer.
    virtual void setRootLayer(WebLayer*) = 0;

    // Attaches/detaches a CompositorAnimationTimeline to the layer tree.
    virtual void attachCompositorAnimationTimeline(CompositorAnimationTimeline*) = 0;
    virtual void detachCompositorAnimationTimeline(CompositorAnimationTimeline*) = 0;
};

DEFINE_TYPE_CASTS(WebFrameWidgetBase, WebFrameWidget, widget, true, true);

} // namespace blink

#endif
