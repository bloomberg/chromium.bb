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

    virtual void willBeDestroyed() OVERRIDE;

    virtual void setName(const String&);
    virtual int debugID() const;

    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    virtual void removeFromParent();

    virtual void setPosition(const FloatPoint&);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);

    virtual void setChildrenTransform(const TransformationMatrix&);

    virtual void setPreserves3D(bool);
    virtual void setMasksToBounds(bool);
    virtual void setDrawsContent(bool);
    virtual void setContentsVisible(bool);
    virtual void setMaskLayer(GraphicsLayer*);

    virtual void setBackgroundColor(const Color&);

    virtual void setContentsOpaque(bool);
    virtual void setBackfaceVisibility(bool);

    virtual void setReplicatedByLayer(GraphicsLayer*);

    virtual void setOpacity(float);

    // Returns true if filter can be rendered by the compositor
    virtual bool setFilters(const FilterOperations&);
    void setBackgroundFilters(const FilterOperations&);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);
    virtual void setContentsNeedsDisplay();

    virtual void setContentsRect(const IntRect&);

    virtual void setContentsToImage(Image*) OVERRIDE;
    virtual void setContentsToMedia(PlatformLayer*) OVERRIDE;
    virtual void setContentsToCanvas(PlatformLayer*) OVERRIDE;
    virtual void setContentsToSolidColor(const Color&) OVERRIDE;
    virtual bool hasContentsLayer() const { return m_contentsLayer; }

    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const CSSAnimationData*, const String&, double timeOffset);
    virtual void pauseAnimation(const String& animationName, double timeOffset);
    virtual void removeAnimation(const String& animationName);
    virtual void suspendAnimations(double wallClockTime);
    virtual void resumeAnimations();

    void setLinkHighlight(LinkHighlightClient*);
    // Next function for testing purposes.
    LinkHighlightClient* linkHighlight() { return m_linkHighlight; }

    virtual WebKit::WebLayer* platformLayer() const;

    virtual void setAppliesPageScale(bool appliesScale) OVERRIDE;
    virtual bool appliesPageScale() const OVERRIDE;

    void setScrollableArea(ScrollableArea* scrollableArea) { m_scrollableArea = scrollableArea; }
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    // GraphicsContextPainter implementation.
    virtual void paint(GraphicsContext&, const IntRect& clip) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double startTime) OVERRIDE;
    virtual void notifyAnimationFinished(double finishTime) OVERRIDE;

    // WebLayerScrollClient implementation.
    virtual void didScroll() OVERRIDE;

    WebKit::WebContentLayer* contentLayer() const { return m_layer.get(); }

    // Exposed for tests.
    WebKit::WebLayer* contentsLayer() const { return m_contentsLayer; }

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

    virtual void setAnimationDelegateForLayer(WebKit::WebLayer*) OVERRIDE;
};

} // namespace WebCore

#endif
