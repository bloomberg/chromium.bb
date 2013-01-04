// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_layer_impl.h"

#include "SkMatrix44.h"
#ifdef LOG
#undef LOG
#endif
#include "base/string_util.h"
#include "cc/active_animation.h"
#include "cc/layer.h"
#include "cc/region.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"
#include "web_animation_impl.h"

using cc::ActiveAnimation;
using cc::Layer;

namespace {
gfx::Transform convertWebTransformationMatrixToTransform(const WebKit::WebTransformationMatrix& matrix)
{
    // TODO(jamesr): When gfx::Transform provides a constructor that does not
    // initialize the matrix, use that.
    gfx::Transform transform;
    transform.matrix().setDouble(0, 0, matrix.m11());
    transform.matrix().setDouble(0, 1, matrix.m21());
    transform.matrix().setDouble(0, 2, matrix.m31());
    transform.matrix().setDouble(0, 3, matrix.m41());
    transform.matrix().setDouble(1, 0, matrix.m12());
    transform.matrix().setDouble(1, 1, matrix.m22());
    transform.matrix().setDouble(1, 2, matrix.m32());
    transform.matrix().setDouble(1, 3, matrix.m42());
    transform.matrix().setDouble(2, 0, matrix.m13());
    transform.matrix().setDouble(2, 1, matrix.m23());
    transform.matrix().setDouble(2, 2, matrix.m33());
    transform.matrix().setDouble(2, 3, matrix.m43());
    transform.matrix().setDouble(3, 0, matrix.m14());
    transform.matrix().setDouble(3, 1, matrix.m24());
    transform.matrix().setDouble(3, 2, matrix.m34());
    transform.matrix().setDouble(3, 3, matrix.m44());
    return transform;
}
}  // namespace


namespace WebKit {

WebLayerImpl::WebLayerImpl()
    : m_layer(Layer::create())
{
}

WebLayerImpl::WebLayerImpl(scoped_refptr<Layer> layer)
    : m_layer(layer)
{
}


WebLayerImpl::~WebLayerImpl()
{
    m_layer->clearRenderSurface();
    m_layer->setLayerAnimationDelegate(0);
}

int WebLayerImpl::id() const
{
    return m_layer->id();
}

void WebLayerImpl::invalidateRect(const WebFloatRect& rect)
{
    m_layer->setNeedsDisplayRect(rect);
}

void WebLayerImpl::invalidate()
{
    m_layer->setNeedsDisplay();
}

void WebLayerImpl::addChild(WebLayer* child)
{
    m_layer->addChild(static_cast<WebLayerImpl*>(child)->layer());
}

void WebLayerImpl::insertChild(WebLayer* child, size_t index)
{
    m_layer->insertChild(static_cast<WebLayerImpl*>(child)->layer(), index);
}

void WebLayerImpl::replaceChild(WebLayer* reference, WebLayer* newLayer)
{
    m_layer->replaceChild(static_cast<WebLayerImpl*>(reference)->layer(), static_cast<WebLayerImpl*>(newLayer)->layer());
}

void WebLayerImpl::removeFromParent()
{
    m_layer->removeFromParent();
}

void WebLayerImpl::removeAllChildren()
{
    m_layer->removeAllChildren();
}

void WebLayerImpl::setAnchorPoint(const WebFloatPoint& anchorPoint)
{
    m_layer->setAnchorPoint(anchorPoint);
}

WebFloatPoint WebLayerImpl::anchorPoint() const
{
    return m_layer->anchorPoint();
}

void WebLayerImpl::setAnchorPointZ(float anchorPointZ)
{
    m_layer->setAnchorPointZ(anchorPointZ);
}

float WebLayerImpl::anchorPointZ() const
{
    return m_layer->anchorPointZ();
}

void WebLayerImpl::setBounds(const WebSize& size)
{
    m_layer->setBounds(size);
}

WebSize WebLayerImpl::bounds() const
{
    return m_layer->bounds();
}

void WebLayerImpl::setMasksToBounds(bool masksToBounds)
{
    m_layer->setMasksToBounds(masksToBounds);
}

bool WebLayerImpl::masksToBounds() const
{
    return m_layer->masksToBounds();
}

void WebLayerImpl::setMaskLayer(WebLayer* maskLayer)
{
    m_layer->setMaskLayer(maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : 0);
}

void WebLayerImpl::setReplicaLayer(WebLayer* replicaLayer)
{
    m_layer->setReplicaLayer(replicaLayer ? static_cast<WebLayerImpl*>(replicaLayer)->layer() : 0);
}

void WebLayerImpl::setOpacity(float opacity)
{
    m_layer->setOpacity(opacity);
}

float WebLayerImpl::opacity() const
{
    return m_layer->opacity();
}

void WebLayerImpl::setOpaque(bool opaque)
{
    m_layer->setContentsOpaque(opaque);
}

bool WebLayerImpl::opaque() const
{
    return m_layer->contentsOpaque();
}

void WebLayerImpl::setPosition(const WebFloatPoint& position)
{
    m_layer->setPosition(position);
}

WebFloatPoint WebLayerImpl::position() const
{
    return m_layer->position();
}

void WebLayerImpl::setSublayerTransform(const SkMatrix44& matrix)
{
    gfx::Transform subLayerTransform;
    subLayerTransform.matrix() = matrix;
    m_layer->setSublayerTransform(subLayerTransform);
}

void WebLayerImpl::setSublayerTransform(const WebTransformationMatrix& matrix)
{
    m_layer->setSublayerTransform(convertWebTransformationMatrixToTransform(matrix));
}

SkMatrix44 WebLayerImpl::sublayerTransform() const
{
    return m_layer->sublayerTransform().matrix();
}

void WebLayerImpl::setTransform(const SkMatrix44& matrix)
{
    gfx::Transform transform;
    transform.matrix() = matrix;
    m_layer->setTransform(transform);
}

void WebLayerImpl::setTransform(const WebTransformationMatrix& matrix)
{
    m_layer->setTransform(convertWebTransformationMatrixToTransform(matrix));
}

SkMatrix44 WebLayerImpl::transform() const
{
    return m_layer->transform().matrix();
}

void WebLayerImpl::setDrawsContent(bool drawsContent)
{
    m_layer->setIsDrawable(drawsContent);
}

bool WebLayerImpl::drawsContent() const
{
    return m_layer->drawsContent();
}

void WebLayerImpl::setPreserves3D(bool preserve3D)
{
    m_layer->setPreserves3D(preserve3D);
}

void WebLayerImpl::setUseParentBackfaceVisibility(bool useParentBackfaceVisibility)
{
    m_layer->setUseParentBackfaceVisibility(useParentBackfaceVisibility);
}

void WebLayerImpl::setBackgroundColor(WebColor color)
{
    m_layer->setBackgroundColor(color);
}

WebColor WebLayerImpl::backgroundColor() const
{
    return m_layer->backgroundColor();
}

void WebLayerImpl::setFilters(const WebFilterOperations& filters)
{
    m_layer->setFilters(filters);
}

void WebLayerImpl::setBackgroundFilters(const WebFilterOperations& filters)
{
    m_layer->setBackgroundFilters(filters);
}

void WebLayerImpl::setFilter(SkImageFilter* filter)
{
    SkSafeRef(filter); // Claim a reference for the compositor.
    m_layer->setFilter(skia::AdoptRef(filter));
}

void WebLayerImpl::setDebugName(WebString name)
{
    m_layer->setDebugName(UTF16ToASCII(string16(name.data(), name.length())));
}

void WebLayerImpl::setAnimationDelegate(WebAnimationDelegate* delegate)
{
    m_layer->setLayerAnimationDelegate(delegate);
}

bool WebLayerImpl::addAnimation(WebAnimation* animation)
{
    return m_layer->addAnimation(static_cast<WebAnimationImpl*>(animation)->cloneToAnimation());
}

void WebLayerImpl::removeAnimation(int animationId)
{
    m_layer->removeAnimation(animationId);
}

void WebLayerImpl::removeAnimation(int animationId, WebAnimation::TargetProperty targetProperty)
{
    m_layer->layerAnimationController()->removeAnimation(animationId, static_cast<ActiveAnimation::TargetProperty>(targetProperty));
}

void WebLayerImpl::pauseAnimation(int animationId, double timeOffset)
{
    m_layer->pauseAnimation(animationId, timeOffset);
}

void WebLayerImpl::suspendAnimations(double monotonicTime)
{
    m_layer->suspendAnimations(monotonicTime);
}

void WebLayerImpl::resumeAnimations(double monotonicTime)
{
    m_layer->resumeAnimations(monotonicTime);
}

bool WebLayerImpl::hasActiveAnimation()
{
    return m_layer->hasActiveAnimation();
}

void WebLayerImpl::transferAnimationsTo(WebLayer* other)
{
    DCHECK(other);
    static_cast<WebLayerImpl*>(other)->m_layer->setLayerAnimationController(m_layer->releaseLayerAnimationController());
}

void WebLayerImpl::setForceRenderSurface(bool forceRenderSurface)
{
    m_layer->setForceRenderSurface(forceRenderSurface);
}

void WebLayerImpl::setScrollPosition(WebPoint position)
{
    m_layer->setScrollOffset(gfx::Point(position).OffsetFromOrigin());
}

WebPoint WebLayerImpl::scrollPosition() const
{
    return gfx::PointAtOffsetFromOrigin(m_layer->scrollOffset());
}

void WebLayerImpl::setMaxScrollPosition(WebSize maxScrollPosition)
{
    m_layer->setMaxScrollOffset(maxScrollPosition);
}

WebSize WebLayerImpl::maxScrollPosition() const
{
    return m_layer->maxScrollOffset();
}

void WebLayerImpl::setScrollable(bool scrollable)
{
    m_layer->setScrollable(scrollable);
}

bool WebLayerImpl::scrollable() const
{
    return m_layer->scrollable();
}

void WebLayerImpl::setHaveWheelEventHandlers(bool haveWheelEventHandlers)
{
    m_layer->setHaveWheelEventHandlers(haveWheelEventHandlers);
}

bool WebLayerImpl::haveWheelEventHandlers() const
{
    return m_layer->haveWheelEventHandlers();
}

void WebLayerImpl::setShouldScrollOnMainThread(bool shouldScrollOnMainThread)
{
    m_layer->setShouldScrollOnMainThread(shouldScrollOnMainThread);
}

bool WebLayerImpl::shouldScrollOnMainThread() const
{
    return m_layer->shouldScrollOnMainThread();
}

void WebLayerImpl::setNonFastScrollableRegion(const WebVector<WebRect>& rects)
{
    cc::Region region;
    for (size_t i = 0; i < rects.size(); ++i)
        region.Union(rects[i]);
    m_layer->setNonFastScrollableRegion(region);
}

WebVector<WebRect> WebLayerImpl::nonFastScrollableRegion() const
{
    size_t numRects = 0;
    for (cc::Region::Iterator regionRects(m_layer->nonFastScrollableRegion()); regionRects.has_rect(); regionRects.next())
        ++numRects;

    WebVector<WebRect> result(numRects);
    size_t i = 0;
    for (cc::Region::Iterator regionRects(m_layer->nonFastScrollableRegion()); regionRects.has_rect(); regionRects.next()) {
        result[i] = regionRects.rect();
        ++i;
    }
    return result;
}

void WebLayerImpl::setTouchEventHandlerRegion(const WebVector<WebRect>& rects)
{
    cc::Region region;
    for (size_t i = 0; i < rects.size(); ++i)
        region.Union(rects[i]);
    m_layer->setTouchEventHandlerRegion(region);
}

WebVector<WebRect> WebLayerImpl::touchEventHandlerRegion() const
{
    size_t numRects = 0;
    for (cc::Region::Iterator regionRects(m_layer->touchEventHandlerRegion()); regionRects.has_rect(); regionRects.next())
        ++numRects;


    WebVector<WebRect> result(numRects);
    size_t i = 0;
    for (cc::Region::Iterator regionRects(m_layer->touchEventHandlerRegion()); regionRects.has_rect(); regionRects.next()) {
        result[i] = regionRects.rect();
        ++i;
    }
    return result;
}

void WebLayerImpl::setIsContainerForFixedPositionLayers(bool enable)
{
    m_layer->setIsContainerForFixedPositionLayers(enable);
}

bool WebLayerImpl::isContainerForFixedPositionLayers() const
{
    return m_layer->isContainerForFixedPositionLayers();
}

void WebLayerImpl::setFixedToContainerLayer(bool enable)
{
    m_layer->setFixedToContainerLayer(enable);
}

bool WebLayerImpl::fixedToContainerLayer() const
{
    return m_layer->fixedToContainerLayer();
}

void WebLayerImpl::setScrollClient(WebLayerScrollClient* scrollClient)
{
    m_layer->setLayerScrollClient(scrollClient);
}

Layer* WebLayerImpl::layer() const
{
    return m_layer.get();
}

} // namespace WebKit
