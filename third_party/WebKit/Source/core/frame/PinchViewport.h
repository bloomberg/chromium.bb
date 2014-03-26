/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef PinchViewport_h
#define PinchViewport_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebSize.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class WebLayerTreeView;
class WebScrollbarLayer;
}

namespace WebCore {

class FrameHost;
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerFactory;
class IntRect;
class IntSize;

class PinchViewport FINAL : public GraphicsLayerClient, public ScrollableArea {
public:
    PinchViewport(FrameHost&);
    virtual ~PinchViewport();

    void attachToLayerTree(GraphicsLayer*, GraphicsLayerFactory*);
    GraphicsLayer* rootGraphicsLayer()
    {
        return m_innerViewportContainerLayer.get();
    }

    void setLocation(const IntPoint&);
    void setSize(const IntSize&);

    void registerLayersWithTreeView(blink::WebLayerTreeView*) const;
    void clearLayersForTreeView(blink::WebLayerTreeView*) const;

    IntRect visibleRect() const { return m_visibleRect; }
private:
    // ScrollableArea implementation
    virtual bool isActive() const OVERRIDE { return false; }
    virtual int scrollSize(ScrollbarOrientation) const OVERRIDE;
    virtual bool isScrollCornerVisible() const OVERRIDE { return false; }
    virtual IntRect scrollCornerRect() const OVERRIDE { return IntRect(); }
    virtual IntPoint scrollPosition() const OVERRIDE { return visibleRect().location(); }
    virtual IntPoint minimumScrollPosition() const OVERRIDE;
    virtual IntPoint maximumScrollPosition() const OVERRIDE;
    virtual int visibleHeight() const OVERRIDE { return visibleRect().height(); };
    virtual int visibleWidth() const OVERRIDE { return visibleRect().width(); };
    virtual IntSize contentsSize() const OVERRIDE;
    virtual bool scrollbarsCanBeActive() const OVERRIDE { return false; }
    virtual IntRect scrollableAreaBoundingBox() const OVERRIDE;
    virtual bool userInputScrollable(ScrollbarOrientation) const OVERRIDE { return true; }
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const OVERRIDE { return false; }
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) OVERRIDE;
    virtual void invalidateScrollCornerRect(const IntRect&) OVERRIDE { }
    virtual void setScrollOffset(const IntPoint&) OVERRIDE;
    virtual GraphicsLayer* layerForContainer() const OVERRIDE;
    virtual GraphicsLayer* layerForScrolling() const OVERRIDE;
    virtual GraphicsLayer* layerForHorizontalScrollbar() const OVERRIDE;
    virtual GraphicsLayer* layerForVerticalScrollbar() const OVERRIDE;

    // GraphicsLayerClient implementation.
    virtual void notifyAnimationStarted(const GraphicsLayer*, double monotonicTime) OVERRIDE;
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) OVERRIDE;
    virtual String debugName(const GraphicsLayer*) OVERRIDE;

    void setupScrollbar(blink::WebScrollbar::Orientation);

    FrameHost& m_frameHost;
    OwnPtr<GraphicsLayer> m_innerViewportContainerLayer;
    OwnPtr<GraphicsLayer> m_pageScaleLayer;
    OwnPtr<GraphicsLayer> m_innerViewportScrollLayer;
    OwnPtr<GraphicsLayer> m_overlayScrollbarHorizontal;
    OwnPtr<GraphicsLayer> m_overlayScrollbarVertical;
    OwnPtr<blink::WebScrollbarLayer> m_webOverlayScrollbarHorizontal;
    OwnPtr<blink::WebScrollbarLayer> m_webOverlayScrollbarVertical;

    IntRect m_visibleRect;
};

} // namespace WebCore

#endif // PinchViewport_h
