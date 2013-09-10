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

#include "SkImageFilter.h"
#include "SkMatrix44.h"
#include "core/platform/ScrollableArea.h"
#include "core/platform/graphics/FloatPoint.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsLayerFactory.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/graphics/chromium/AnimationTranslationUtil.h"
#include "core/platform/graphics/chromium/TransformSkMatrix44Conversions.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"
#include "core/platform/graphics/transforms/RotateTransformOperation.h"
#include "core/platform/text/TextStream.h"

#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

#include "public/platform/Platform.h"
#include "public/platform/WebAnimation.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFilterOperations.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using WebKit::Platform;
using WebKit::WebAnimation;
using WebKit::WebFilterOperations;
using WebKit::WebLayer;
using WebKit::WebPoint;

namespace WebCore {

typedef HashMap<const GraphicsLayer*, Vector<FloatRect> > RepaintMap;
static RepaintMap& repaintRectMap()
{
    DEFINE_STATIC_LOCAL(RepaintMap, map, ());
    return map;
}

void KeyframeValueList::insert(PassOwnPtr<const AnimationValue> value)
{
    for (size_t i = 0; i < m_values.size(); ++i) {
        const AnimationValue* curValue = m_values[i].get();
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

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient* client)
{
    return factory->createGraphicsLayer(client);
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
    , m_paintingPhase(GraphicsLayerPaintAllWithOverflowClip)
    , m_contentsOrientation(CompositingCoordinatesTopDown)
    , m_parent(0)
    , m_maskLayer(0)
    , m_replicaLayer(0)
    , m_replicatedLayer(0)
    , m_paintCount(0)
    , m_contentsLayer(0)
    , m_contentsLayerId(0)
    , m_linkHighlight(0)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_scrollableArea(0)
    , m_compositingReasons(WebKit::CompositingReasonUnknown)
{
#ifndef NDEBUG
    if (m_client)
        m_client->verifyNotPainting();
#endif

    m_opaqueRectTrackingContentLayerDelegate = adoptPtr(new OpaqueRectTrackingContentLayerDelegate(this));
    m_layer = adoptPtr(Platform::current()->compositorSupport()->createContentLayer(m_opaqueRectTrackingContentLayerDelegate.get()));
    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    m_layer->layer()->setWebLayerClient(this);
    m_layer->setAutomaticallyComputeRasterScale(true);
}

GraphicsLayer::~GraphicsLayer()
{
    if (m_linkHighlight) {
        m_linkHighlight->clearCurrentGraphicsLayer();
        m_linkHighlight = 0;
    }

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

    resetTrackedRepaints();
    ASSERT(!m_parent);
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
        addChildInternal(newChildren[i]);

    updateChildList();

    return true;
}

void GraphicsLayer::addChildInternal(GraphicsLayer* childLayer)
{
    ASSERT(childLayer != this);

    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.append(childLayer);

    // Don't call updateChildList here, this function is used in cases where it
    // should not be called until all children are processed.
}

void GraphicsLayer::addChild(GraphicsLayer* childLayer)
{
    addChildInternal(childLayer);
    updateChildList();
}

void GraphicsLayer::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    ASSERT(childLayer != this);

    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.insert(index, childLayer);

    updateChildList();
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

    updateChildList();
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

    updateChildList();
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

        updateChildList();
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

    platformLayer()->removeFromParent();
}

void GraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    // FIXME: this could probably be a full early exit.
    if (m_replicaLayer != layer) {
        if (m_replicaLayer)
            m_replicaLayer->setReplicatedLayer(0);

        if (layer)
            layer->setReplicatedLayer(this);

        m_replicaLayer = layer;
    }

    WebLayer* webReplicaLayer = layer ? layer->platformLayer() : 0;
    platformLayer()->setReplicaLayer(webReplicaLayer);
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

void GraphicsLayer::paintGraphicsLayerContents(GraphicsContext& context, const IntRect& clip)
{
    if (!m_client)
        return;
    incrementPaintCount();
    m_client->paintContents(this, context, m_paintingPhase, clip);
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
        if (type == TransformOperation::RotateX
            || type == TransformOperation::RotateY
            || type == TransformOperation::RotateZ
            || type == TransformOperation::Rotate3D) {
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

void GraphicsLayer::updateChildList()
{
    WebLayer* childHost = m_layer->layer();
    childHost->removeAllChildren();

    clearContentsLayerIfUnregistered();

    if (m_contentsLayer) {
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
            m_contentsLayerId = 0;
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
        m_contentsLayer->setWebLayerClient(this);
        m_contentsLayer->setAnchorPoint(FloatPoint(0, 0));
        m_contentsLayer->setUseParentBackfaceVisibility(true);

        // It is necessary to call setDrawsContent as soon as we receive the new contentsLayer, for
        // the correctness of early exit conditions in setDrawsContent() and setContentsVisible().
        m_contentsLayer->setDrawsContent(m_contentsVisible);

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer->layer()->insertChild(m_contentsLayer, 0);
    }
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

void GraphicsLayer::dumpLayer(TextStream& ts, int indent, LayerTreeFlags flags) const
{
    writeIndent(ts, indent);
    ts << "(" << "GraphicsLayer";

    if (flags & LayerTreeIncludesDebugInfo) {
        ts << " " << static_cast<void*>(const_cast<GraphicsLayer*>(this));
        ts << " \"" << m_client->debugName(this) << "\"";
    }

    ts << "\n";
    dumpProperties(ts, indent, flags);
    writeIndent(ts, indent);
    ts << ")\n";
}

void GraphicsLayer::dumpProperties(TextStream& ts, int indent, LayerTreeFlags flags) const
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

    if (flags & LayerTreeIncludesDebugInfo) {
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
        if (flags & LayerTreeIncludesDebugInfo)
            ts << " " << m_replicaLayer;
        ts << ")\n";
        m_replicaLayer->dumpLayer(ts, indent + 2, flags);
    }

    if (m_replicatedLayer) {
        writeIndent(ts, indent + 1);
        ts << "(replicated layer";
        if (flags & LayerTreeIncludesDebugInfo)
            ts << " " << m_replicatedLayer;
        ts << ")\n";
    }

    if ((flags & LayerTreeIncludesRepaintRects) && repaintRectMap().contains(this) && !repaintRectMap().get(this).isEmpty()) {
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

    if ((flags & LayerTreeIncludesPaintingPhases) && paintingPhase()) {
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

    dumpAdditionalProperties(ts, indent, flags);

    if (m_children.size()) {
        writeIndent(ts, indent + 1);
        ts << "(children " << m_children.size() << "\n";

        unsigned i;
        for (i = 0; i < m_children.size(); i++)
            m_children[i]->dumpLayer(ts, indent + 2, flags);
        writeIndent(ts, indent + 1);
        ts << ")\n";
    }
}

String GraphicsLayer::layerTreeAsText(LayerTreeFlags flags) const
{
    TextStream ts;

    dumpLayer(ts, 0, flags);
    return ts.release();
}

WebKit::WebString GraphicsLayer::debugName(WebKit::WebLayer* webLayer)
{
    String name;
    if (!m_client)
        return name;

    if (webLayer == m_contentsLayer) {
        name = "ContentsLayer for " + m_client->debugName(this);
    } else if (m_linkHighlight && webLayer == m_linkHighlight->layer()) {
        name = "LinkHighlight for " + m_client->debugName(this);
    } else if (webLayer == m_layer->layer()) {
        name = m_client->debugName(this);
    } else {
        ASSERT_NOT_REACHED();
    }
    return name;
}

void GraphicsLayer::setCompositingReasons(WebKit::WebCompositingReasons reasons)
{
    m_compositingReasons = reasons;
    m_layer->layer()->setCompositingReasons(reasons);
}

void GraphicsLayer::setPosition(const FloatPoint& point)
{
    m_position = point;
    platformLayer()->setPosition(m_position);
}

void GraphicsLayer::setAnchorPoint(const FloatPoint3D& point)
{
    m_anchorPoint = point;
    platformLayer()->setAnchorPoint(FloatPoint(m_anchorPoint.x(), m_anchorPoint.y()));
    platformLayer()->setAnchorPointZ(m_anchorPoint.z());
}

void GraphicsLayer::setSize(const FloatSize& size)
{
    // We are receiving negative sizes here that cause assertions to fail in the compositor. Clamp them to 0 to
    // avoid those assertions.
    // FIXME: This should be an ASSERT instead, as negative sizes should not exist in WebCore.
    FloatSize clampedSize = size;
    if (clampedSize.width() < 0 || clampedSize.height() < 0)
        clampedSize = FloatSize();

    if (clampedSize == m_size)
        return;

    m_size = clampedSize;

    m_layer->layer()->setBounds(flooredIntSize(m_size));
    // Note that we don't resize m_contentsLayer. It's up the caller to do that.
}

void GraphicsLayer::setTransform(const TransformationMatrix& transform)
{
    m_transform = transform;
    platformLayer()->setTransform(TransformSkMatrix44Conversions::convert(m_transform));
}

void GraphicsLayer::setChildrenTransform(const TransformationMatrix& transform)
{
    m_childrenTransform = transform;
    platformLayer()->setSublayerTransform(TransformSkMatrix44Conversions::convert(m_childrenTransform));
}

void GraphicsLayer::setPreserves3D(bool preserves3D)
{
    if (preserves3D == m_preserves3D)
        return;

    m_preserves3D = preserves3D;
    m_layer->layer()->setPreserves3D(m_preserves3D);
}

void GraphicsLayer::setMasksToBounds(bool masksToBounds)
{
    m_masksToBounds = masksToBounds;
    m_layer->layer()->setMasksToBounds(m_masksToBounds);
}

void GraphicsLayer::setDrawsContent(bool drawsContent)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (drawsContent == m_drawsContent)
        return;

    m_drawsContent = drawsContent;
    updateLayerIsDrawable();
}

void GraphicsLayer::setContentsVisible(bool contentsVisible)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (contentsVisible == m_contentsVisible)
        return;

    m_contentsVisible = contentsVisible;
    updateLayerIsDrawable();
}

void GraphicsLayer::setBackgroundColor(const Color& color)
{
    if (color == m_backgroundColor)
        return;

    m_backgroundColor = color;
    m_layer->layer()->setBackgroundColor(m_backgroundColor.rgb());
}

void GraphicsLayer::setContentsOpaque(bool opaque)
{
    m_contentsOpaque = opaque;
    m_layer->layer()->setOpaque(m_contentsOpaque);
    m_opaqueRectTrackingContentLayerDelegate->setOpaque(m_contentsOpaque);
}

void GraphicsLayer::setMaskLayer(GraphicsLayer* maskLayer)
{
    if (maskLayer == m_maskLayer)
        return;

    m_maskLayer = maskLayer;
    WebLayer* maskWebLayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    m_layer->layer()->setMaskLayer(maskWebLayer);
}

void GraphicsLayer::setBackfaceVisibility(bool visible)
{
    m_backfaceVisibility = visible;
    m_layer->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayer::setOpacity(float opacity)
{
    float clampedOpacity = std::max(std::min(opacity, 1.0f), 0.0f);
    m_opacity = clampedOpacity;
    platformLayer()->setOpacity(opacity);
}

void GraphicsLayer::setContentsNeedsDisplay()
{
    if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
        contentsLayer->invalidate();
        addRepaintRect(contentsRect());
    }
}

void GraphicsLayer::setNeedsDisplay()
{
    if (drawsContent()) {
        m_layer->layer()->invalidate();
        addRepaintRect(FloatRect(FloatPoint(), m_size));
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayer::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent()) {
        m_layer->layer()->invalidateRect(rect);
        addRepaintRect(rect);
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayer::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    m_contentsRect = rect;
    updateContentsRect();
}

void GraphicsLayer::setContentsToImage(Image* image)
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

void GraphicsLayer::setContentsToNinePatch(Image* image, const IntRect& aperture)
{
    if (m_ninePatchLayer) {
        unregisterContentsLayer(m_ninePatchLayer->layer());
        m_ninePatchLayer.clear();
    }
    RefPtr<NativeImageSkia> nativeImage = image ? image->nativeImageForCurrentFrame() : 0;
    if (nativeImage) {
        m_ninePatchLayer = adoptPtr(Platform::current()->compositorSupport()->createNinePatchLayer());
        m_ninePatchLayer->setBitmap(nativeImage->bitmap(), aperture);
        m_ninePatchLayer->layer()->setOpaque(image->currentFrameKnownToBeOpaque());
        registerContentsLayer(m_ninePatchLayer->layer());
    }
    setContentsTo(ContentsLayerForNinePatch, m_ninePatchLayer ? m_ninePatchLayer->layer() : 0);
}

void GraphicsLayer::setContentsToCanvas(WebLayer* layer)
{
    setContentsTo(ContentsLayerForCanvas, layer);
}

void GraphicsLayer::setContentsToMedia(WebLayer* layer)
{
    setContentsTo(ContentsLayerForVideo, layer);
}

bool GraphicsLayer::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const CSSAnimationData* animation, const String& animationName, double timeOffset)
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

void GraphicsLayer::pauseAnimation(const String& animationName, double timeOffset)
{
    if (m_animationIdMap.contains(animationName))
        platformLayer()->pauseAnimation(m_animationIdMap.get(animationName), timeOffset);
}

void GraphicsLayer::removeAnimation(const String& animationName)
{
    if (m_animationIdMap.contains(animationName))
        platformLayer()->removeAnimation(m_animationIdMap.get(animationName));
}

void GraphicsLayer::suspendAnimations(double wallClockTime)
{
    // |wallClockTime| is in the wrong time base. Need to convert here.
    // FIXME: find a more reliable way to do this.
    double monotonicTime = wallClockTime + monotonicallyIncreasingTime() - currentTime();
    platformLayer()->suspendAnimations(monotonicTime);
}

void GraphicsLayer::resumeAnimations()
{
    platformLayer()->resumeAnimations(monotonicallyIncreasingTime());
}

WebLayer* GraphicsLayer::platformLayer() const
{
    return m_layer->layer();
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
                webFilters.appendGrayscaleFilter(amount);
                break;
            case FilterOperation::SEPIA:
                webFilters.appendSepiaFilter(amount);
                break;
            case FilterOperation::SATURATE:
                webFilters.appendSaturateFilter(amount);
                break;
            case FilterOperation::HUE_ROTATE:
                webFilters.appendHueRotateFilter(amount);
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
                webFilters.appendInvertFilter(amount);
                break;
            case FilterOperation::OPACITY:
                webFilters.appendOpacityFilter(amount);
                break;
            case FilterOperation::BRIGHTNESS:
                webFilters.appendBrightnessFilter(amount);
                break;
            case FilterOperation::CONTRAST:
                webFilters.appendContrastFilter(amount);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::BLUR: {
            float pixelRadius = static_cast<const BlurFilterOperation*>(&op)->stdDeviation().getFloatValue();
            webFilters.appendBlurFilter(pixelRadius);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation& dropShadowOp = *static_cast<const DropShadowFilterOperation*>(&op);
            webFilters.appendDropShadowFilter(WebPoint(dropShadowOp.x(), dropShadowOp.y()), dropShadowOp.stdDeviation(), dropShadowOp.color().rgb());
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

bool GraphicsLayer::setFilters(const FilterOperations& filters)
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
            m_filters = FilterOperations();
            return false;
        }
        SkiaImageFilterBuilder builder;
        FilterOutsets outsets = filters.outsets();
        builder.setCropOffset(FloatSize(outsets.left(), outsets.top()));
        RefPtr<SkImageFilter> imageFilter = builder.build(filters);
        m_layer->layer()->setFilter(imageFilter.get());
    } else {
        OwnPtr<WebFilterOperations> webFilters = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
        if (!copyWebCoreFilterOperationsToWebFilterOperations(filters, *webFilters)) {
            // Make sure the filters are removed from the platform layer, as they are
            // going to fallback to software mode.
            webFilters->clear();
            m_layer->layer()->setFilters(*webFilters);
            m_filters = FilterOperations();
            return false;
        }
        m_layer->layer()->setFilters(*webFilters);
    }

    m_filters = filters;
    return true;
}

void GraphicsLayer::setBackgroundFilters(const FilterOperations& filters)
{
    OwnPtr<WebFilterOperations> webFilters = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
    if (!copyWebCoreFilterOperationsToWebFilterOperations(filters, *webFilters))
        return;
    m_layer->layer()->setBackgroundFilters(*webFilters);
}

void GraphicsLayer::setLinkHighlight(LinkHighlightClient* linkHighlight)
{
    m_linkHighlight = linkHighlight;
    if (m_linkHighlight)
        m_linkHighlight->layer()->setWebLayerClient(this);
    updateChildList();
}

void GraphicsLayer::setScrollableArea(ScrollableArea* scrollableArea, bool isMainFrame)
{
    if (m_scrollableArea == scrollableArea)
        return;

    m_scrollableArea = scrollableArea;

    // Main frame scrolling may involve pinch zoom and gets routed through
    // WebViewImpl explicitly rather than via GraphicsLayer::didScroll.
    if (isMainFrame)
        m_layer->layer()->setScrollClient(0);
    else
        m_layer->layer()->setScrollClient(this);
}

void GraphicsLayer::paint(GraphicsContext& context, const IntRect& clip)
{
    paintGraphicsLayerContents(context, clip);
}


void GraphicsLayer::notifyAnimationStarted(double startTime)
{
    if (m_client)
        m_client->notifyAnimationStarted(this, startTime);
}

void GraphicsLayer::notifyAnimationFinished(double)
{
    // Do nothing.
}

void GraphicsLayer::didScroll()
{
    if (m_scrollableArea)
        m_scrollableArea->scrollToOffsetWithoutAnimation(m_scrollableArea->minimumScrollPosition() + toIntSize(m_layer->layer()->scrollPosition()));
}

} // namespace WebCore

#ifndef NDEBUG
void showGraphicsLayerTree(const WebCore::GraphicsLayer* layer)
{
    if (!layer)
        return;

    String output = layer->layerTreeAsText(WebCore::LayerTreeIncludesDebugInfo);
    fprintf(stderr, "%s\n", output.utf8().data());
}
#endif
