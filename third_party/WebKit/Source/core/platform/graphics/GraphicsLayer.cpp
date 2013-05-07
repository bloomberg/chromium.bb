/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "core/platform/graphics/GraphicsLayer.h"

#include "core/platform/PlatformMemoryInstrumentation.h"
#include "core/platform/graphics/FloatPoint.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/graphics/chromium/TransformSkMatrix44Conversions.h"
#include "core/platform/graphics/transforms/RotateTransformOperation.h"
#include "core/platform/text/TextStream.h"

#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebSize.h>

#include <wtf/HashMap.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#ifndef NDEBUG
#include <stdio.h>
#endif

using WebKit::WebLayer;
using WebKit::Platform;

namespace WebCore {

typedef HashMap<const GraphicsLayer*, Vector<FloatRect> > RepaintMap;
static RepaintMap& repaintRectMap()
{
    DEFINE_STATIC_LOCAL(RepaintMap, map, ());
    return map;
}

void KeyframeValueList::insert(const AnimationValue* value)
{
    for (size_t i = 0; i < m_values.size(); ++i) {
        const AnimationValue* curValue = m_values[i];
        if (curValue->keyTime() == value->keyTime()) {
            ASSERT_NOT_REACHED();
            // insert after
            m_values.insert(i + 1, value);
            return;
        }
        if (curValue->keyTime() > value->keyTime()) {
            // insert before
            m_values.insert(i, value);
            return;
        }
    }
    
    m_values.append(value);
}

GraphicsLayer::GraphicsLayer(GraphicsLayerClient* client)
    : m_client(client)
    , m_anchorPoint(0.5f, 0.5f, 0)
    , m_opacity(1)
    , m_zPosition(0)
    , m_contentsOpaque(false)
    , m_preserves3D(false)
    , m_backfaceVisibility(true)
    , m_masksToBounds(false)
    , m_drawsContent(false)
    , m_contentsVisible(true)
    , m_maintainsPixelAlignment(false)
    , m_appliesPageScale(false)
    , m_showDebugBorder(false)
    , m_showRepaintCounter(false)
    , m_paintingPhase(GraphicsLayerPaintAllWithOverflowClip)
    , m_contentsOrientation(CompositingCoordinatesTopDown)
    , m_parent(0)
    , m_maskLayer(0)
    , m_replicaLayer(0)
    , m_replicatedLayer(0)
    , m_repaintCount(0)
    , m_contentsLayer(0)
    , m_contentsLayerId(0)
    , m_linkHighlight(0)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_inSetChildren(false)
    , m_scrollableArea(0)
{
#ifndef NDEBUG
    if (m_client)
        m_client->verifyNotPainting();
#endif
}

GraphicsLayer::~GraphicsLayer()
{
    resetTrackedRepaints();
    ASSERT(!m_parent); // willBeDestroyed should have been called already.
}

void GraphicsLayer::willBeDestroyed()
{
#ifndef NDEBUG
    if (m_client)
        m_client->verifyNotPainting();
#endif

    if (m_replicaLayer)
        m_replicaLayer->setReplicatedLayer(0);

    if (m_replicatedLayer)
        m_replicatedLayer->setReplicatedByLayer(0);

    removeAllChildren();
    removeFromParent();
}

void GraphicsLayer::setParent(GraphicsLayer* layer)
{
    ASSERT(!layer || !layer->hasAncestor(this));
    m_parent = layer;
}

bool GraphicsLayer::hasAncestor(GraphicsLayer* ancestor) const
{
    for (GraphicsLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr == ancestor)
            return true;
    }
    
    return false;
}

bool GraphicsLayer::setChildren(const Vector<GraphicsLayer*>& newChildren)
{
    // If the contents of the arrays are the same, nothing to do.
    if (newChildren == m_children)
        return false;

    removeAllChildren();
    
    size_t listSize = newChildren.size();
    for (size_t i = 0; i < listSize; ++i)
        addChild(newChildren[i]);
    
    return true;
}

void GraphicsLayer::addChild(GraphicsLayer* childLayer)
{
    ASSERT(childLayer != this);
    
    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.append(childLayer);
}

void GraphicsLayer::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    ASSERT(childLayer != this);

    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.insert(index, childLayer);
}

void GraphicsLayer::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    ASSERT(childLayer != this);
    childLayer->removeFromParent();

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i, childLayer);
            found = true;
            break;
        }
    }

    childLayer->setParent(this);

    if (!found)
        m_children.append(childLayer);
}

void GraphicsLayer::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    childLayer->removeFromParent();
    ASSERT(childLayer != this);

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i+1, childLayer);
            found = true;
            break;
        }
    }

    childLayer->setParent(this);

    if (!found)
        m_children.append(childLayer);
}

bool GraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    ASSERT(!newChild->parent());
    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (oldChild == m_children[i]) {
            m_children[i] = newChild;
            found = true;
            break;
        }
    }
    if (found) {
        oldChild->setParent(0);

        newChild->removeFromParent();
        newChild->setParent(this);
        return true;
    }
    return false;
}

void GraphicsLayer::removeAllChildren()
{
    while (m_children.size()) {
        GraphicsLayer* curLayer = m_children[0];
        ASSERT(curLayer->parent());
        curLayer->removeFromParent();
    }
}

void GraphicsLayer::removeFromParent()
{
    if (m_parent) {
        unsigned i;
        for (i = 0; i < m_parent->m_children.size(); i++) {
            if (this == m_parent->m_children[i]) {
                m_parent->m_children.remove(i);
                break;
            }
        }

        setParent(0);
    }
}

void GraphicsLayer::noteDeviceOrPageScaleFactorChangedIncludingDescendants()
{
    deviceOrPageScaleFactorChanged();

    if (m_maskLayer)
        m_maskLayer->deviceOrPageScaleFactorChanged();

    if (m_replicaLayer)
        m_replicaLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i)
        childLayers[i]->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

void GraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    if (m_replicaLayer == layer)
        return;

    if (m_replicaLayer)
        m_replicaLayer->setReplicatedLayer(0);

    if (layer)
        layer->setReplicatedLayer(this);

    m_replicaLayer = layer;
}

void GraphicsLayer::setOffsetFromRenderer(const IntSize& offset, ShouldSetNeedsDisplay shouldSetNeedsDisplay)
{
    if (offset == m_offsetFromRenderer)
        return;

    m_offsetFromRenderer = offset;

    // If the compositing layer offset changes, we need to repaint.
    if (shouldSetNeedsDisplay == SetNeedsDisplay)
        setNeedsDisplay();
}

void GraphicsLayer::setBackgroundColor(const Color& color)
{
    m_backgroundColor = color;
}

void GraphicsLayer::paintGraphicsLayerContents(GraphicsContext& context, const IntRect& clip)
{
    if (m_client) {
        LayoutSize offset = offsetFromRenderer();
        context.translate(-offset);

        LayoutRect clipRect(clip);
        clipRect.move(offset);

        m_client->paintContents(this, context, m_paintingPhase, pixelSnappedIntRect(clipRect));
    }
}

String GraphicsLayer::animationNameForTransition(AnimatedPropertyID property)
{
    // | is not a valid identifier character in CSS, so this can never conflict with a keyframe identifier.
    StringBuilder id;
    id.appendLiteral("-|transition");
    id.appendNumber(static_cast<int>(property));
    id.append('-');
    return id.toString();
}

void GraphicsLayer::suspendAnimations(double)
{
}

void GraphicsLayer::resumeAnimations()
{
}

void GraphicsLayer::getDebugBorderInfo(Color& color, float& width) const
{
    if (drawsContent()) {
        color = Color(0, 128, 32, 128); // normal layer: green
        width = 2;
        return;
    }
    
    if (masksToBounds()) {
        color = Color(128, 255, 255, 48); // masking layer: pale blue
        width = 20;
        return;
    }
        
    color = Color(255, 255, 0, 192); // container: yellow
    width = 2;
}

void GraphicsLayer::updateDebugIndicators()
{
    if (!isShowingDebugBorder())
        return;

    Color borderColor;
    float width = 0;
    getDebugBorderInfo(borderColor, width);
    setDebugBorder(borderColor, width);
}

void GraphicsLayer::setZPosition(float position)
{
    m_zPosition = position;
}

float GraphicsLayer::accumulatedOpacity() const
{
    if (!preserves3D())
        return 1;
        
    return m_opacity * (parent() ? parent()->accumulatedOpacity() : 1);
}

void GraphicsLayer::distributeOpacity(float accumulatedOpacity)
{
    // If this is a transform layer we need to distribute our opacity to all our children
    
    // Incoming accumulatedOpacity is the contribution from our parent(s). We mutiply this by our own
    // opacity to get the total contribution
    accumulatedOpacity *= m_opacity;
    
    setOpacityInternal(accumulatedOpacity);
    
    if (preserves3D()) {
        size_t numChildren = children().size();
        for (size_t i = 0; i < numChildren; ++i)
            children()[i]->distributeOpacity(accumulatedOpacity);
    }
}

static inline const FilterOperations* filterOperationsAt(const KeyframeValueList& valueList, size_t index)
{
    return static_cast<const FilterAnimationValue*>(valueList.at(index))->value();
}

int GraphicsLayer::validateFilterOperations(const KeyframeValueList& valueList)
{
    ASSERT(valueList.property() == AnimatedPropertyWebkitFilter);

    if (valueList.size() < 2)
        return -1;

    // Empty filters match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    for ( ; firstIndex < valueList.size(); ++firstIndex) {
        if (filterOperationsAt(valueList, firstIndex)->operations().size() > 0)
            break;
    }

    if (firstIndex >= valueList.size())
        return -1;

    const FilterOperations* firstVal = filterOperationsAt(valueList, firstIndex);
    
    for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
        const FilterOperations* val = filterOperationsAt(valueList, i);
        
        // An emtpy filter list matches anything.
        if (val->operations().isEmpty())
            continue;
        
        if (!firstVal->operationsMatch(*val))
            return -1;
    }
    
    return firstIndex;
}

// An "invalid" list is one whose functions don't match, and therefore has to be animated as a Matrix
// The hasBigRotation flag will always return false if isValid is false. Otherwise hasBigRotation is 
// true if the rotation between any two keyframes is >= 180 degrees.

static inline const TransformOperations* operationsAt(const KeyframeValueList& valueList, size_t index)
{
    return static_cast<const TransformAnimationValue*>(valueList.at(index))->value();
}

int GraphicsLayer::validateTransformOperations(const KeyframeValueList& valueList, bool& hasBigRotation)
{
    ASSERT(valueList.property() == AnimatedPropertyWebkitTransform);

    hasBigRotation = false;
    
    if (valueList.size() < 2)
        return -1;
    
    // Empty transforms match anything, so find the first non-empty entry as the reference.
    size_t firstIndex = 0;
    for ( ; firstIndex < valueList.size(); ++firstIndex) {
        if (operationsAt(valueList, firstIndex)->operations().size() > 0)
            break;
    }
    
    if (firstIndex >= valueList.size())
        return -1;
        
    const TransformOperations* firstVal = operationsAt(valueList, firstIndex);
    
    // See if the keyframes are valid.
    for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
        const TransformOperations* val = operationsAt(valueList, i);
        
        // An emtpy transform list matches anything.
        if (val->operations().isEmpty())
            continue;
            
        if (!firstVal->operationsMatch(*val))
            return -1;
    }

    // Keyframes are valid, check for big rotations.    
    double lastRotAngle = 0.0;
    double maxRotAngle = -1.0;
        
    for (size_t j = 0; j < firstVal->operations().size(); ++j) {
        TransformOperation::OperationType type = firstVal->operations().at(j)->getOperationType();
        
        // if this is a rotation entry, we need to see if any angle differences are >= 180 deg
        if (type == TransformOperation::ROTATE_X ||
            type == TransformOperation::ROTATE_Y ||
            type == TransformOperation::ROTATE_Z ||
            type == TransformOperation::ROTATE_3D) {
            lastRotAngle = static_cast<RotateTransformOperation*>(firstVal->operations().at(j).get())->angle();
            
            if (maxRotAngle < 0)
                maxRotAngle = fabs(lastRotAngle);
            
            for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
                const TransformOperations* val = operationsAt(valueList, i);
                double rotAngle = val->operations().isEmpty() ? 0 : (static_cast<RotateTransformOperation*>(val->operations().at(j).get())->angle());
                double diffAngle = fabs(rotAngle - lastRotAngle);
                if (diffAngle > maxRotAngle)
                    maxRotAngle = diffAngle;
                lastRotAngle = rotAngle;
            }
        }
    }
    
    hasBigRotation = maxRotAngle >= 180.0;
    
    return firstIndex;
}

void GraphicsLayer::updateNames()
{
    String debugName = "Layer for " + m_nameBase;
    m_layer->layer()->setDebugName(debugName);

    if (m_transformLayer) {
        String debugName = "TransformLayer for " + m_nameBase;
        m_transformLayer->setDebugName(debugName);
    }
    if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
        String debugName = "ContentsLayer for " + m_nameBase;
        contentsLayer->setDebugName(debugName);
    }
    if (m_linkHighlight) {
        String debugName = "LinkHighlight for " + m_nameBase;
        m_linkHighlight->layer()->setDebugName(debugName);
    }
}

void GraphicsLayer::updateChildList()
{
    WebLayer* childHost = m_transformLayer ? m_transformLayer.get() : m_layer->layer();
    childHost->removeAllChildren();

    clearContentsLayerIfUnregistered();

    if (m_transformLayer) {
        // Add the primary layer first. Even if we have negative z-order children, the primary layer always comes behind.
        childHost->addChild(m_layer->layer());
    } else if (m_contentsLayer) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        childHost->addChild(m_contentsLayer);
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayer* curChild = childLayers[i];

        childHost->addChild(curChild->platformLayer());
    }

    if (m_linkHighlight)
        childHost->addChild(m_linkHighlight->layer());

    if (m_transformLayer && m_contentsLayer) {
        // If we have a transform layer, then the contents layer is parented in the
        // primary layer (which is itself a child of the transform layer).
        m_layer->layer()->removeAllChildren();
        m_layer->layer()->addChild(m_contentsLayer);
    }
}

void GraphicsLayer::updateLayerPosition()
{
    platformLayer()->setPosition(m_position);
}

void GraphicsLayer::updateLayerSize()
{
    IntSize layerSize(m_size.width(), m_size.height());
    if (m_transformLayer) {
        m_transformLayer->setBounds(layerSize);
        m_layer->layer()->setPosition(FloatPoint());
    }

    m_layer->layer()->setBounds(layerSize);

    // Note that we don't resize m_contentsLayer-> It's up the caller to do that.
}

void GraphicsLayer::updateAnchorPoint()
{
    platformLayer()->setAnchorPoint(FloatPoint(m_anchorPoint.x(), m_anchorPoint.y()));
    platformLayer()->setAnchorPointZ(m_anchorPoint.z());
}

void GraphicsLayer::updateTransform()
{
    platformLayer()->setTransform(TransformSkMatrix44Conversions::convert(m_transform));
}

void GraphicsLayer::updateChildrenTransform()
{
    platformLayer()->setSublayerTransform(TransformSkMatrix44Conversions::convert(m_childrenTransform));
}

void GraphicsLayer::updateMasksToBounds()
{
    m_layer->layer()->setMasksToBounds(m_masksToBounds);
}

void GraphicsLayer::updateLayerPreserves3D()
{
    if (m_preserves3D && !m_transformLayer) {
        m_transformLayer = adoptPtr(Platform::current()->compositorSupport()->createLayer());
        m_transformLayer->setPreserves3D(true);
        setAnimationDelegateForLayer(m_transformLayer.get());
        m_layer->layer()->transferAnimationsTo(m_transformLayer.get());

        // Copy the position from this layer.
        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        m_layer->layer()->setPosition(FloatPoint::zero());

        m_layer->layer()->setAnchorPoint(FloatPoint(0.5f, 0.5f));
        m_layer->layer()->setTransform(SkMatrix44::I());

        // Set the old layer to opacity of 1. Further down we will set the opacity on the transform layer.
        m_layer->layer()->setOpacity(1);

        // Move this layer to be a child of the transform layer.
        if (parent())
            parent()->platformLayer()->replaceChild(m_layer->layer(), m_transformLayer.get());
        m_transformLayer->addChild(m_layer->layer());

        updateChildList();
    } else if (!m_preserves3D && m_transformLayer) {
        // Replace the transformLayer in the parent with this layer.
        m_layer->layer()->removeFromParent();
        if (parent())
            parent()->platformLayer()->replaceChild(m_transformLayer.get(), m_layer->layer());

        setAnimationDelegateForLayer(m_layer->layer());
        m_transformLayer->transferAnimationsTo(m_layer->layer());

        // Release the transform layer.
        m_transformLayer->setAnimationDelegate(0);
        m_transformLayer.clear();

        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        updateChildList();
    }

    m_layer->layer()->setPreserves3D(m_preserves3D);
    platformLayer()->setOpacity(m_opacity);
    updateNames();
}

void GraphicsLayer::updateLayerIsDrawable()
{
    // For the rest of the accelerated compositor code, there is no reason to make a
    // distinction between drawsContent and contentsVisible. So, for m_layer->layer(), these two
    // flags are combined here. m_contentsLayer shouldn't receive the drawsContent flag
    // so it is only given contentsVisible.

    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    if (WebLayer* contentsLayer = contentsLayerIfRegistered())
        contentsLayer->setDrawsContent(m_contentsVisible);

    if (m_drawsContent) {
        m_layer->layer()->invalidate();
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayer::updateLayerBackgroundColor()
{
    m_layer->layer()->setBackgroundColor(m_backgroundColor.rgb());
}

void GraphicsLayer::updateContentsRect()
{
    WebLayer* contentsLayer = contentsLayerIfRegistered();
    if (!contentsLayer)
        return;

    contentsLayer->setPosition(FloatPoint(m_contentsRect.x(), m_contentsRect.y()));
    contentsLayer->setBounds(IntSize(m_contentsRect.width(), m_contentsRect.height()));
}

static HashSet<int>* s_registeredLayerSet;

void GraphicsLayer::registerContentsLayer(WebLayer* layer)
{
    if (!s_registeredLayerSet)
        s_registeredLayerSet = new HashSet<int>;
    if (s_registeredLayerSet->contains(layer->id()))
        CRASH();
    s_registeredLayerSet->add(layer->id());
}

void GraphicsLayer::unregisterContentsLayer(WebLayer* layer)
{
    ASSERT(s_registeredLayerSet);
    if (!s_registeredLayerSet->contains(layer->id()))
        CRASH();
    s_registeredLayerSet->remove(layer->id());
}

void GraphicsLayer::setContentsTo(ContentsLayerPurpose purpose, WebLayer* layer)
{
    bool childrenChanged = false;
    if (layer) {
        ASSERT(s_registeredLayerSet);
        if (!s_registeredLayerSet->contains(layer->id()))
            CRASH();
        if (m_contentsLayerId != layer->id()) {
            setupContentsLayer(layer);
            m_contentsLayerPurpose = purpose;
            childrenChanged = true;
        }
        updateContentsRect();
    } else {
        if (m_contentsLayer) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer = 0;
        }
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayer::setupContentsLayer(WebLayer* contentsLayer)
{
    m_contentsLayer = contentsLayer;
    m_contentsLayerId = m_contentsLayer->id();

    if (m_contentsLayer) {
        m_contentsLayer->setAnchorPoint(FloatPoint(0, 0));
        m_contentsLayer->setUseParentBackfaceVisibility(true);

        // It is necessary to call setDrawsContent as soon as we receive the new contentsLayer, for
        // the correctness of early exit conditions in setDrawsContent() and setContentsVisible().
        m_contentsLayer->setDrawsContent(m_contentsVisible);

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer->layer()->insertChild(m_contentsLayer, 0);
    }
    updateNames();
}

void GraphicsLayer::clearContentsLayerIfUnregistered()
{
    if (!m_contentsLayerId || s_registeredLayerSet->contains(m_contentsLayerId))
        return;

    m_contentsLayer = 0;
    m_contentsLayerId = 0;
}

WebLayer* GraphicsLayer::contentsLayerIfRegistered()
{
    clearContentsLayerIfUnregistered();
    return m_contentsLayer;
}

double GraphicsLayer::backingStoreMemoryEstimate() const
{
    if (!drawsContent())
        return 0;
    
    // Effects of page and device scale are ignored; subclasses should override to take these into account.
    return static_cast<double>(4 * size().width()) * size().height();
}

void GraphicsLayer::resetTrackedRepaints()
{
    repaintRectMap().remove(this);
}

void GraphicsLayer::addRepaintRect(const FloatRect& repaintRect)
{
    if (m_client->isTrackingRepaints()) {
        FloatRect largestRepaintRect(FloatPoint(), m_size);
        largestRepaintRect.intersect(repaintRect);
        RepaintMap::iterator repaintIt = repaintRectMap().find(this);
        if (repaintIt == repaintRectMap().end()) {
            Vector<FloatRect> repaintRects;
            repaintRects.append(largestRepaintRect);
            repaintRectMap().set(this, repaintRects);
        } else {
            Vector<FloatRect>& repaintRects = repaintIt->value;
            repaintRects.append(largestRepaintRect);
        }
    }
}

void GraphicsLayer::writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void GraphicsLayer::dumpLayer(TextStream& ts, int indent, LayerTreeAsTextBehavior behavior) const
{
    writeIndent(ts, indent);
    ts << "(" << "GraphicsLayer";

    if (behavior & LayerTreeAsTextDebug) {
        ts << " " << static_cast<void*>(const_cast<GraphicsLayer*>(this));
        ts << " \"" << m_name << "\"";
    }

    ts << "\n";
    dumpProperties(ts, indent, behavior);
    writeIndent(ts, indent);
    ts << ")\n";
}

void GraphicsLayer::dumpProperties(TextStream& ts, int indent, LayerTreeAsTextBehavior behavior) const
{
    if (m_position != FloatPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(position " << m_position.x() << " " << m_position.y() << ")\n";
    }

    if (m_boundsOrigin != FloatPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(bounds origin " << m_boundsOrigin.x() << " " << m_boundsOrigin.y() << ")\n";
    }

    if (m_anchorPoint != FloatPoint3D(0.5f, 0.5f, 0)) {
        writeIndent(ts, indent + 1);
        ts << "(anchor " << m_anchorPoint.x() << " " << m_anchorPoint.y() << ")\n";
    }

    if (m_size != IntSize()) {
        writeIndent(ts, indent + 1);
        ts << "(bounds " << m_size.width() << " " << m_size.height() << ")\n";
    }

    if (m_opacity != 1) {
        writeIndent(ts, indent + 1);
        ts << "(opacity " << m_opacity << ")\n";
    }
    
    if (m_contentsOpaque) {
        writeIndent(ts, indent + 1);
        ts << "(contentsOpaque " << m_contentsOpaque << ")\n";
    }

    if (m_preserves3D) {
        writeIndent(ts, indent + 1);
        ts << "(preserves3D " << m_preserves3D << ")\n";
    }

    if (m_drawsContent) {
        writeIndent(ts, indent + 1);
        ts << "(drawsContent " << m_drawsContent << ")\n";
    }

    if (!m_contentsVisible) {
        writeIndent(ts, indent + 1);
        ts << "(contentsVisible " << m_contentsVisible << ")\n";
    }

    if (!m_backfaceVisibility) {
        writeIndent(ts, indent + 1);
        ts << "(backfaceVisibility " << (m_backfaceVisibility ? "visible" : "hidden") << ")\n";
    }

    if (behavior & LayerTreeAsTextDebug) {
        writeIndent(ts, indent + 1);
        ts << "(";
        if (m_client)
            ts << "client " << static_cast<void*>(m_client);
        else
            ts << "no client";
        ts << ")\n";
    }

    if (m_backgroundColor.isValid() && m_backgroundColor != Color::transparent) {
        writeIndent(ts, indent + 1);
        ts << "(backgroundColor " << m_backgroundColor.nameForRenderTreeAsText() << ")\n";
    }

    if (!m_transform.isIdentity()) {
        writeIndent(ts, indent + 1);
        ts << "(transform ";
        ts << "[" << m_transform.m11() << " " << m_transform.m12() << " " << m_transform.m13() << " " << m_transform.m14() << "] ";
        ts << "[" << m_transform.m21() << " " << m_transform.m22() << " " << m_transform.m23() << " " << m_transform.m24() << "] ";
        ts << "[" << m_transform.m31() << " " << m_transform.m32() << " " << m_transform.m33() << " " << m_transform.m34() << "] ";
        ts << "[" << m_transform.m41() << " " << m_transform.m42() << " " << m_transform.m43() << " " << m_transform.m44() << "])\n";
    }

    // Avoid dumping the sublayer transform on the root layer, because it's used for geometry flipping, whose behavior
    // differs between platforms.
    if (parent() && !m_childrenTransform.isIdentity()) {
        writeIndent(ts, indent + 1);
        ts << "(childrenTransform ";
        ts << "[" << m_childrenTransform.m11() << " " << m_childrenTransform.m12() << " " << m_childrenTransform.m13() << " " << m_childrenTransform.m14() << "] ";
        ts << "[" << m_childrenTransform.m21() << " " << m_childrenTransform.m22() << " " << m_childrenTransform.m23() << " " << m_childrenTransform.m24() << "] ";
        ts << "[" << m_childrenTransform.m31() << " " << m_childrenTransform.m32() << " " << m_childrenTransform.m33() << " " << m_childrenTransform.m34() << "] ";
        ts << "[" << m_childrenTransform.m41() << " " << m_childrenTransform.m42() << " " << m_childrenTransform.m43() << " " << m_childrenTransform.m44() << "])\n";
    }

    if (m_replicaLayer) {
        writeIndent(ts, indent + 1);
        ts << "(replica layer";
        if (behavior & LayerTreeAsTextDebug)
            ts << " " << m_replicaLayer;
        ts << ")\n";
        m_replicaLayer->dumpLayer(ts, indent + 2, behavior);
    }

    if (m_replicatedLayer) {
        writeIndent(ts, indent + 1);
        ts << "(replicated layer";
        if (behavior & LayerTreeAsTextDebug)
            ts << " " << m_replicatedLayer;
        ts << ")\n";
    }

    if (behavior & LayerTreeAsTextIncludeRepaintRects && repaintRectMap().contains(this) && !repaintRectMap().get(this).isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(repaint rects\n";
        for (size_t i = 0; i < repaintRectMap().get(this).size(); ++i) {
            if (repaintRectMap().get(this)[i].isEmpty())
                continue;
            writeIndent(ts, indent + 2);
            ts << "(rect ";
            ts << repaintRectMap().get(this)[i].x() << " ";
            ts << repaintRectMap().get(this)[i].y() << " ";
            ts << repaintRectMap().get(this)[i].width() << " ";
            ts << repaintRectMap().get(this)[i].height();
            ts << ")\n";
        }
        writeIndent(ts, indent + 1);
        ts << ")\n";
    }

    if (behavior & LayerTreeAsTextIncludePaintingPhases && paintingPhase()) {
        writeIndent(ts, indent + 1);
        ts << "(paintingPhases\n";
        if (paintingPhase() & GraphicsLayerPaintBackground) {
            writeIndent(ts, indent + 2);
            ts << "GraphicsLayerPaintBackground\n";
        }
        if (paintingPhase() & GraphicsLayerPaintForeground) {
            writeIndent(ts, indent + 2);
            ts << "GraphicsLayerPaintForeground\n";
        }
        if (paintingPhase() & GraphicsLayerPaintMask) {
            writeIndent(ts, indent + 2);
            ts << "GraphicsLayerPaintMask\n";
        }
        if (paintingPhase() & GraphicsLayerPaintOverflowContents) {
            writeIndent(ts, indent + 2);
            ts << "GraphicsLayerPaintOverflowContents\n";
        }
        if (paintingPhase() & GraphicsLayerPaintCompositedScroll) {
            writeIndent(ts, indent + 2);
            ts << "GraphicsLayerPaintCompositedScroll\n";
        }
        writeIndent(ts, indent + 1);
        ts << ")\n";
    }

    dumpAdditionalProperties(ts, indent, behavior);
    
    if (m_children.size()) {
        writeIndent(ts, indent + 1);
        ts << "(children " << m_children.size() << "\n";
        
        unsigned i;
        for (i = 0; i < m_children.size(); i++)
            m_children[i]->dumpLayer(ts, indent + 2, behavior);
        writeIndent(ts, indent + 1);
        ts << ")\n";
    }
}

String GraphicsLayer::layerTreeAsText(LayerTreeAsTextBehavior behavior) const
{
    TextStream ts;

    dumpLayer(ts, 0, behavior);
    return ts.release();
}

void GraphicsLayer::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::Layers);
    info.addMember(m_children, "children");
    info.addMember(m_parent, "parent");
    info.addMember(m_maskLayer, "maskLayer");
    info.addMember(m_replicaLayer, "replicaLayer");
    info.addMember(m_replicatedLayer, "replicatedLayer");
    info.ignoreMember(m_client);
    info.addMember(m_name, "name");
}

} // namespace WebCore

#ifndef NDEBUG
void showGraphicsLayerTree(const WebCore::GraphicsLayer* layer)
{
    if (!layer)
        return;

    String output = layer->layerTreeAsText(LayerTreeAsTextDebug | LayerTreeAsTextIncludeVisibleRects);
    fprintf(stderr, "%s\n", output.utf8().data());
}
#endif
