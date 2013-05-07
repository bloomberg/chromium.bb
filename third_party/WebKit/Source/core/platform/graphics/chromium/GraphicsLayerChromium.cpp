/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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


/** FIXME
 * This file borrows code heavily from platform/graphics/win/GraphicsLayerCACF.cpp
 * (and hence it includes both copyrights)
 * Ideally the common code (mostly the code that keeps track of the layer hierarchy)
 * should be kept separate and shared between platforms. It would be a well worthwhile
 * effort once the Windows implementation (binaries and headers) of CoreAnimation is
 * checked in to the WebKit repository. Until then only Apple can make this happen.
 */

#include "config.h"

#include "core/platform/graphics/chromium/GraphicsLayerChromium.h"

#include "SkImageFilter.h"
#include "SkMatrix44.h"
#include "core/platform/FloatConversion.h"
#include "core/platform/PlatformMemoryInstrumentation.h"
#include "core/platform/ScrollableArea.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsLayerFactory.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/chromium/AnimationTranslationUtil.h"
#include "core/platform/graphics/chromium/TransformSkMatrix44Conversions.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"
#include "core/platform/graphics/skia/PlatformContextSkia.h"
#include <public/Platform.h>
#include <public/WebAnimation.h>
#include <public/WebCompositorSupport.h>
#include <public/WebContentLayer.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebImageLayer.h>
#include <public/WebSize.h>
#include <public/WebSolidColorLayer.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace std;
using namespace WebKit;

namespace WebCore {

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient* client)
{
    return factory->createGraphicsLayer(client);
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

GraphicsLayerChromium::GraphicsLayerChromium(GraphicsLayerClient* client)
    : GraphicsLayer(client)
{
    m_opaqueRectTrackingContentLayerDelegate = adoptPtr(new OpaqueRectTrackingContentLayerDelegate(this));
    m_layer = adoptPtr(Platform::current()->compositorSupport()->createContentLayer(m_opaqueRectTrackingContentLayerDelegate.get()));
    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    m_layer->layer()->setScrollClient(this);
    m_layer->setAutomaticallyComputeRasterScale(true);
}

GraphicsLayerChromium::~GraphicsLayerChromium()
{
    willBeDestroyed();
}

void GraphicsLayerChromium::willBeDestroyed()
{
    if (m_linkHighlight) {
        m_linkHighlight->clearCurrentGraphicsLayer();
        m_linkHighlight = 0;
    }

    GraphicsLayer::willBeDestroyed();
}

void GraphicsLayerChromium::setName(const String& inName)
{
    m_nameBase = inName;
    String name = String::format("GraphicsLayer(%p) ", this) + inName;
    GraphicsLayer::setName(name);
    updateNames();
}

int GraphicsLayerChromium::debugID() const
{
    return m_layer ? m_layer->layer()->id() : DebugIDNoCompositedLayer;
}

bool GraphicsLayerChromium::setChildren(const Vector<GraphicsLayer*>& children)
{
    m_inSetChildren = true;
    bool childrenChanged = GraphicsLayer::setChildren(children);

    if (childrenChanged)
        updateChildList();
    m_inSetChildren = false;

    return childrenChanged;
}

void GraphicsLayerChromium::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    if (!m_inSetChildren)
        updateChildList();
}

void GraphicsLayerChromium::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    updateChildList();
}

void GraphicsLayerChromium::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    updateChildList();
}

void GraphicsLayerChromium::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer *sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    updateChildList();
}

bool GraphicsLayerChromium::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        updateChildList();
        return true;
    }
    return false;
}

void GraphicsLayerChromium::removeFromParent()
{
    GraphicsLayer::removeFromParent();
    platformLayer()->removeFromParent();
}

void GraphicsLayerChromium::setPosition(const FloatPoint& point)
{
    GraphicsLayer::setPosition(point);
    updateLayerPosition();
}

void GraphicsLayerChromium::setAnchorPoint(const FloatPoint3D& point)
{
    GraphicsLayer::setAnchorPoint(point);
    updateAnchorPoint();
}

void GraphicsLayerChromium::setSize(const FloatSize& size)
{
    // We are receiving negative sizes here that cause assertions to fail in the compositor. Clamp them to 0 to
    // avoid those assertions.
    // FIXME: This should be an ASSERT instead, as negative sizes should not exist in WebCore.
    FloatSize clampedSize = size;
    if (clampedSize.width() < 0 || clampedSize.height() < 0)
        clampedSize = FloatSize();

    if (clampedSize == m_size)
        return;

    GraphicsLayer::setSize(clampedSize);
    updateLayerSize();
}

void GraphicsLayerChromium::setTransform(const TransformationMatrix& transform)
{
    GraphicsLayer::setTransform(transform);
    updateTransform();
}

void GraphicsLayerChromium::setChildrenTransform(const TransformationMatrix& transform)
{
    GraphicsLayer::setChildrenTransform(transform);
    updateChildrenTransform();
}

void GraphicsLayerChromium::setPreserves3D(bool preserves3D)
{
    if (preserves3D == m_preserves3D)
        return;

    GraphicsLayer::setPreserves3D(preserves3D);
    updateLayerPreserves3D();
}

void GraphicsLayerChromium::setMasksToBounds(bool masksToBounds)
{
    GraphicsLayer::setMasksToBounds(masksToBounds);
    updateMasksToBounds();
}

void GraphicsLayerChromium::setDrawsContent(bool drawsContent)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    updateLayerIsDrawable();
}

void GraphicsLayerChromium::setContentsVisible(bool contentsVisible)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (contentsVisible == m_contentsVisible)
        return;

    GraphicsLayer::setContentsVisible(contentsVisible);
    updateLayerIsDrawable();
}

void GraphicsLayerChromium::setBackgroundColor(const Color& color)
{
    if (color == m_backgroundColor)
        return;

    GraphicsLayer::setBackgroundColor(color);
    updateLayerBackgroundColor();
}

void GraphicsLayerChromium::setContentsOpaque(bool opaque)
{
    GraphicsLayer::setContentsOpaque(opaque);
    m_layer->layer()->setOpaque(m_contentsOpaque);
    m_opaqueRectTrackingContentLayerDelegate->setOpaque(m_contentsOpaque);
}

static bool copyWebCoreFilterOperationsToWebFilterOperations(const FilterOperations& filters, WebFilterOperations& webFilters)
{
    for (size_t i = 0; i < filters.size(); ++i) {
        const FilterOperation& op = *filters.at(i);
        switch (op.getOperationType()) {
        case FilterOperation::REFERENCE:
            return false; // Not supported.
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE:
        case FilterOperation::HUE_ROTATE: {
            float amount = static_cast<const BasicColorMatrixFilterOperation*>(&op)->amount();
            switch (op.getOperationType()) {
            case FilterOperation::GRAYSCALE:
                webFilters.append(WebFilterOperation::createGrayscaleFilter(amount));
                break;
            case FilterOperation::SEPIA:
                webFilters.append(WebFilterOperation::createSepiaFilter(amount));
                break;
            case FilterOperation::SATURATE:
                webFilters.append(WebFilterOperation::createSaturateFilter(amount));
                break;
            case FilterOperation::HUE_ROTATE:
                webFilters.append(WebFilterOperation::createHueRotateFilter(amount));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::OPACITY:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST: {
            float amount = static_cast<const BasicComponentTransferFilterOperation*>(&op)->amount();
            switch (op.getOperationType()) {
            case FilterOperation::INVERT:
                webFilters.append(WebFilterOperation::createInvertFilter(amount));
                break;
            case FilterOperation::OPACITY:
                webFilters.append(WebFilterOperation::createOpacityFilter(amount));
                break;
            case FilterOperation::BRIGHTNESS:
                webFilters.append(WebFilterOperation::createBrightnessFilter(amount));
                break;
            case FilterOperation::CONTRAST:
                webFilters.append(WebFilterOperation::createContrastFilter(amount));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::BLUR: {
            float pixelRadius = static_cast<const BlurFilterOperation*>(&op)->stdDeviation().getFloatValue();
            webFilters.append(WebFilterOperation::createBlurFilter(pixelRadius));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation& dropShadowOp = *static_cast<const DropShadowFilterOperation*>(&op);
            webFilters.append(WebFilterOperation::createDropShadowFilter(WebPoint(dropShadowOp.x(), dropShadowOp.y()), dropShadowOp.stdDeviation(), dropShadowOp.color().rgb()));
            break;
        }
        case FilterOperation::CUSTOM:
        case FilterOperation::VALIDATED_CUSTOM:
            return false; // Not supported.
        case FilterOperation::PASSTHROUGH:
        case FilterOperation::NONE:
            break;
        }
    }
    return true;
}

bool GraphicsLayerChromium::setFilters(const FilterOperations& filters)
{
    // FIXME: For now, we only use SkImageFilters if there is a reference
    // filter in the chain. Once all issues have been ironed out, we should
    // switch all filtering over to this path, and remove setFilters() and
    // WebFilterOperations altogether.
    if (filters.hasReferenceFilter()) {
        if (filters.hasCustomFilter()) {
            // Make sure the filters are removed from the platform layer, as they are
            // going to fallback to software mode.
            m_layer->layer()->setFilter(0);
            GraphicsLayer::setFilters(FilterOperations());
            return false;
        }
        SkiaImageFilterBuilder builder;
        SkAutoTUnref<SkImageFilter> imageFilter(builder.build(filters));
        m_layer->layer()->setFilter(imageFilter);
    } else {
        WebFilterOperations webFilters;
        if (!copyWebCoreFilterOperationsToWebFilterOperations(filters, webFilters)) {
            // Make sure the filters are removed from the platform layer, as they are
            // going to fallback to software mode.
            m_layer->layer()->setFilters(WebFilterOperations());
            GraphicsLayer::setFilters(FilterOperations());
            return false;
        }
        m_layer->layer()->setFilters(webFilters);
    }
    return GraphicsLayer::setFilters(filters);
}

void GraphicsLayerChromium::setBackgroundFilters(const FilterOperations& filters)
{
    WebFilterOperations webFilters;
    if (!copyWebCoreFilterOperationsToWebFilterOperations(filters, webFilters))
        return;
    m_layer->layer()->setBackgroundFilters(webFilters);
}

void GraphicsLayerChromium::setMaskLayer(GraphicsLayer* maskLayer)
{
    if (maskLayer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(maskLayer);

    WebLayer* maskWebLayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    m_layer->layer()->setMaskLayer(maskWebLayer);
}

void GraphicsLayerChromium::setBackfaceVisibility(bool visible)
{
    GraphicsLayer::setBackfaceVisibility(visible);
    m_layer->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayerChromium::setOpacity(float opacity)
{
    float clampedOpacity = max(min(opacity, 1.0f), 0.0f);
    GraphicsLayer::setOpacity(clampedOpacity);
    platformLayer()->setOpacity(opacity);
}

void GraphicsLayerChromium::setReplicatedByLayer(GraphicsLayer* layer)
{
    GraphicsLayerChromium* layerChromium = static_cast<GraphicsLayerChromium*>(layer);
    GraphicsLayer::setReplicatedByLayer(layer);

    WebLayer* webReplicaLayer = layerChromium ? layerChromium->platformLayer() : 0;
    platformLayer()->setReplicaLayer(webReplicaLayer);
}


void GraphicsLayerChromium::setContentsNeedsDisplay()
{
    if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
        contentsLayer->invalidate();
        addRepaintRect(contentsRect());
    }
}

void GraphicsLayerChromium::setNeedsDisplay()
{
    if (drawsContent()) {
        m_layer->layer()->invalidate();
        addRepaintRect(FloatRect(FloatPoint(), m_size));
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayerChromium::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent()) {
        m_layer->layer()->invalidateRect(rect);
        addRepaintRect(rect);
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayerChromium::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    GraphicsLayer::setContentsRect(rect);
    updateContentsRect();
}

void GraphicsLayerChromium::setContentsToImage(Image* image)
{
    bool childrenChanged = false;
    RefPtr<NativeImageSkia> nativeImage = image ? image->nativeImageForCurrentFrame() : 0;
    if (nativeImage) {
        if (m_contentsLayerPurpose != ContentsLayerForImage) {
            m_imageLayer = adoptPtr(Platform::current()->compositorSupport()->createImageLayer());
            registerContentsLayer(m_imageLayer->layer());

            setupContentsLayer(m_imageLayer->layer());
            m_contentsLayerPurpose = ContentsLayerForImage;
            childrenChanged = true;
        }
        m_imageLayer->setBitmap(nativeImage->bitmap());
        m_imageLayer->layer()->setOpaque(image->currentFrameKnownToBeOpaque());
        updateContentsRect();
    } else {
        if (m_imageLayer) {
            childrenChanged = true;

            unregisterContentsLayer(m_imageLayer->layer());
            m_imageLayer.clear();
        }
        // The old contents layer will be removed via updateChildList.
        m_contentsLayer = 0;
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayerChromium::setContentsToSolidColor(const Color& color)
{
}

void GraphicsLayerChromium::setContentsToCanvas(PlatformLayer* layer)
{
    setContentsTo(ContentsLayerForCanvas, layer);
}

void GraphicsLayerChromium::setContentsToMedia(PlatformLayer* layer)
{
    setContentsTo(ContentsLayerForVideo, layer);
}

bool GraphicsLayerChromium::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const CSSAnimationData* animation, const String& animationName, double timeOffset)
{
    platformLayer()->setAnimationDelegate(this);

    int animationId = 0;

    if (m_animationIdMap.contains(animationName))
        animationId = m_animationIdMap.get(animationName);

    OwnPtr<WebAnimation> toAdd(createWebAnimation(values, animation, animationId, timeOffset, boxSize));

    if (toAdd) {
        animationId = toAdd->id();
        m_animationIdMap.set(animationName, animationId);

        // Remove any existing animations with the same animation id and target property.
        platformLayer()->removeAnimation(animationId, toAdd->targetProperty());
        return platformLayer()->addAnimation(toAdd.get());
    }

    return false;
}

void GraphicsLayerChromium::pauseAnimation(const String& animationName, double timeOffset)
{
    if (m_animationIdMap.contains(animationName))
        platformLayer()->pauseAnimation(m_animationIdMap.get(animationName), timeOffset);
}

void GraphicsLayerChromium::removeAnimation(const String& animationName)
{
    if (m_animationIdMap.contains(animationName))
        platformLayer()->removeAnimation(m_animationIdMap.get(animationName));
}

void GraphicsLayerChromium::suspendAnimations(double wallClockTime)
{
    // |wallClockTime| is in the wrong time base. Need to convert here.
    // FIXME: find a more reliable way to do this.
    double monotonicTime = wallClockTime + monotonicallyIncreasingTime() - currentTime();
    platformLayer()->suspendAnimations(monotonicTime);
}

void GraphicsLayerChromium::resumeAnimations()
{
    platformLayer()->resumeAnimations(monotonicallyIncreasingTime());
}

void GraphicsLayerChromium::setLinkHighlight(LinkHighlightClient* linkHighlight)
{
    m_linkHighlight = linkHighlight;
    updateChildList();
}

PlatformLayer* GraphicsLayerChromium::platformLayer() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer->layer();
}

void GraphicsLayerChromium::setAppliesPageScale(bool)
{
}

bool GraphicsLayerChromium::appliesPageScale() const
{
    return false;
}

void GraphicsLayerChromium::paint(GraphicsContext& context, const IntRect& clip)
{
    paintGraphicsLayerContents(context, clip);
}

void GraphicsLayerChromium::notifyAnimationStarted(double startTime)
{
    if (m_client)
        m_client->notifyAnimationStarted(this, startTime);
}

void GraphicsLayerChromium::notifyAnimationFinished(double)
{
    // Do nothing.
}

void GraphicsLayerChromium::didScroll()
{
    if (m_scrollableArea)
        m_scrollableArea->scrollToOffsetWithoutAnimation(IntPoint(m_layer->layer()->scrollPosition()));
}

void GraphicsLayerChromium::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::Layers);
    GraphicsLayer::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_nameBase, "nameBase");
    info.addMember(m_layer, "layer");
    info.addMember(m_transformLayer, "transformLayer");
    info.addMember(m_imageLayer, "imageLayer");
    info.addMember(m_contentsLayer, "contentsLayer");
    info.addMember(m_linkHighlight, "linkHighlight");
    info.addMember(m_opaqueRectTrackingContentLayerDelegate, "opaqueRectTrackingContentLayerDelegate");
    info.addMember(m_animationIdMap, "animationIdMap");
    info.addMember(m_scrollableArea, "scrollableArea");
}

void GraphicsLayerChromium::setAnimationDelegateForLayer(WebKit::WebLayer* layer)
{
    layer->setAnimationDelegate(this);
}

} // namespace WebCore
