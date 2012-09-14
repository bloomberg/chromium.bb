// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerImpl_h
#define WebLayerImpl_h

#include <public/WebLayer.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace cc {
class LayerChromium;
}

namespace WebKit {

class WebLayerImpl : public WebLayer {
public:
    WebLayerImpl();
    explicit WebLayerImpl(PassRefPtr<cc::LayerChromium>);
    virtual ~WebLayerImpl();

    // WebLayer implementation.
    virtual int id() const OVERRIDE;
    virtual void invalidateRect(const WebFloatRect&) OVERRIDE;
    virtual void invalidate() OVERRIDE;
    virtual void addChild(WebLayer*) OVERRIDE;
    virtual void insertChild(WebLayer*, size_t index) OVERRIDE;
    virtual void replaceChild(WebLayer* reference, WebLayer* newLayer) OVERRIDE;
    virtual void removeFromParent() OVERRIDE;
    virtual void removeAllChildren() OVERRIDE;
    virtual void setAnchorPoint(const WebFloatPoint&) OVERRIDE;
    virtual WebFloatPoint anchorPoint() const OVERRIDE;
    virtual void setAnchorPointZ(float) OVERRIDE;
    virtual float anchorPointZ() const OVERRIDE;
    virtual void setBounds(const WebSize&) OVERRIDE;
    virtual WebSize bounds() const OVERRIDE;
    virtual void setMasksToBounds(bool) OVERRIDE;
    virtual bool masksToBounds() const OVERRIDE;
    virtual void setMaskLayer(WebLayer*) OVERRIDE;
    virtual void setReplicaLayer(WebLayer*) OVERRIDE;
    virtual void setOpacity(float) OVERRIDE;
    virtual float opacity() const OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;
    virtual bool opaque() const OVERRIDE;
    virtual void setPosition(const WebFloatPoint&) OVERRIDE;
    virtual WebFloatPoint position() const OVERRIDE;
    virtual void setSublayerTransform(const SkMatrix44&) OVERRIDE;
    virtual void setSublayerTransform(const WebTransformationMatrix&) OVERRIDE;
    virtual SkMatrix44 sublayerTransform() const OVERRIDE;
    virtual void setTransform(const SkMatrix44&) OVERRIDE;
    virtual void setTransform(const WebTransformationMatrix&) OVERRIDE;
    virtual SkMatrix44 transform() const OVERRIDE;
    virtual void setDrawsContent(bool) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void setPreserves3D(bool) OVERRIDE;
    virtual void setUseParentBackfaceVisibility(bool) OVERRIDE;
    virtual void setBackgroundColor(WebColor) OVERRIDE;
    virtual void setFilters(const WebFilterOperations&) OVERRIDE;
    virtual void setBackgroundFilters(const WebFilterOperations&) OVERRIDE;
    virtual void setDebugBorderColor(const WebColor&) OVERRIDE;
    virtual void setDebugBorderWidth(float) OVERRIDE;
    virtual void setDebugName(WebString) OVERRIDE;
    virtual void setAnimationDelegate(WebAnimationDelegate*) OVERRIDE;
    virtual bool addAnimation(WebAnimation*) OVERRIDE;
    virtual void removeAnimation(int animationId) OVERRIDE;
    virtual void removeAnimation(int animationId, WebAnimation::TargetProperty) OVERRIDE;
    virtual void pauseAnimation(int animationId, double timeOffset) OVERRIDE;
    virtual void suspendAnimations(double monotonicTime) OVERRIDE;
    virtual void resumeAnimations(double monotonicTime) OVERRIDE;
    virtual bool hasActiveAnimation() OVERRIDE;
    virtual void transferAnimationsTo(WebLayer*) OVERRIDE;
    virtual void setForceRenderSurface(bool) OVERRIDE;
    virtual void setScrollPosition(WebPoint) OVERRIDE;
    virtual WebPoint scrollPosition() const OVERRIDE;
    virtual void setMaxScrollPosition(WebSize) OVERRIDE;
    virtual void setScrollable(bool) OVERRIDE;
    virtual void setHaveWheelEventHandlers(bool) OVERRIDE;
    virtual void setShouldScrollOnMainThread(bool) OVERRIDE;
    virtual void setNonFastScrollableRegion(const WebVector<WebRect>&) OVERRIDE;
    virtual void setIsContainerForFixedPositionLayers(bool) OVERRIDE;
    virtual void setFixedToContainerLayer(bool) OVERRIDE;
    virtual void setScrollClient(WebLayerScrollClient*) OVERRIDE;

    cc::LayerChromium* layer() const;

protected:
    RefPtr<cc::LayerChromium> m_layer;
};

} // namespace WebKit

#endif // WebLayerImpl_h
