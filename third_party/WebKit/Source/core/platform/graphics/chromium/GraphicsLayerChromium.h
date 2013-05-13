/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsLayerChromium_h
#define GraphicsLayerChromium_h

#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/chromium/OpaqueRectTrackingContentLayerDelegate.h"

#include <public/WebAnimationDelegate.h>
#include <public/WebLayerScrollClient.h>
#include <wtf/HashMap.h>

namespace WebKit {
class WebContentLayer;
class WebImageLayer;
class WebSolidColorLayer;
class WebLayer;
}

namespace WebCore {

class Path;
class ScrollableArea;

class GraphicsLayerChromium : public GraphicsLayer, public GraphicsContextPainter, public WebKit::WebAnimationDelegate, public WebKit::WebLayerScrollClient {
public:
    GraphicsLayerChromium(GraphicsLayerClient*);
    virtual ~GraphicsLayerChromium();

    // GraphicsContextPainter implementation.
    virtual void paint(GraphicsContext&, const IntRect& clip) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double startTime) OVERRIDE;
    virtual void notifyAnimationFinished(double finishTime) OVERRIDE;

    // WebLayerScrollClient implementation.
    virtual void didScroll() OVERRIDE;

    virtual void setAnimationDelegateForLayer(WebKit::WebLayer*) OVERRIDE;
};

} // namespace WebCore

#endif
