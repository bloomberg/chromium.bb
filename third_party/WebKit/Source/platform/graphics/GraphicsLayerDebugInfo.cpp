/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "platform/graphics/GraphicsLayerDebugInfo.h"

#include "wtf/text/CString.h"

namespace WebCore {

GraphicsLayerDebugInfo::GraphicsLayerDebugInfo()
    : m_compositingReasons(CompositingReasonNone)
{
}

GraphicsLayerDebugInfo::~GraphicsLayerDebugInfo() { }

void GraphicsLayerDebugInfo::appendAsTraceFormat(blink::WebString* out) const
{
    RefPtr<JSONObject> jsonObject = JSONObject::create();
    appendLayoutRects(jsonObject.get());
    appendCompositingReasons(jsonObject.get());
    appendDebugName(jsonObject.get());
    *out = jsonObject->toJSONString();
}

GraphicsLayerDebugInfo* GraphicsLayerDebugInfo::clone() const
{
    GraphicsLayerDebugInfo* toReturn = new GraphicsLayerDebugInfo();
    for (size_t i = 0; i < m_currentLayoutRects.size(); ++i)
        toReturn->currentLayoutRects().append(m_currentLayoutRects[i]);
    toReturn->setCompositingReasons(m_compositingReasons);
    return toReturn;
}

void GraphicsLayerDebugInfo::appendLayoutRects(JSONObject* jsonObject) const
{
    RefPtr<JSONArray> jsonArray = JSONArray::create();
    for (size_t i = 0; i < m_currentLayoutRects.size(); i++) {
        const LayoutRect& rect = m_currentLayoutRects[i];
        RefPtr<JSONObject> rectContainer = JSONObject::create();
        RefPtr<JSONArray> rectArray = JSONArray::create();
        rectArray->pushNumber(rect.x().toFloat());
        rectArray->pushNumber(rect.y().toFloat());
        rectArray->pushNumber(rect.maxX().toFloat());
        rectArray->pushNumber(rect.maxY().toFloat());
        rectContainer->setArray("geometry_rect", rectArray);
        jsonArray->pushObject(rectContainer);
    }
    jsonObject->setArray("layout_rects", jsonArray);
}

void GraphicsLayerDebugInfo::appendCompositingReasons(JSONObject* jsonObject) const
{
    RefPtr<JSONArray> jsonArray = JSONArray::create();
    if (m_compositingReasons == CompositingReasonNone) {
        jsonArray->pushString("No reasons given");
        jsonObject->setArray("compositing_reasons", jsonArray);
        return;
    }

    if (m_compositingReasons & CompositingReason3DTransform)
        jsonArray->pushString("Has a 3d Transform");

    if (m_compositingReasons & CompositingReasonVideo)
        jsonArray->pushString("Is accelerated video");

    if (m_compositingReasons & CompositingReasonCanvas)
        jsonArray->pushString("Is accelerated canvas");

    if (m_compositingReasons & CompositingReasonPlugin)
        jsonArray->pushString("Is accelerated plugin");

    if (m_compositingReasons & CompositingReasonIFrame)
        jsonArray->pushString("Is accelerated iframe");

    if (m_compositingReasons & CompositingReasonBackfaceVisibilityHidden)
        jsonArray->pushString("Has backface-visibility: hidden");

    if (m_compositingReasons & CompositingReasonAnimation)
        jsonArray->pushString("Has accelerated animation or transition");

    if (m_compositingReasons & CompositingReasonFilters)
        jsonArray->pushString("Has accelerated filters");

    if (m_compositingReasons & CompositingReasonPositionFixed)
        jsonArray->pushString("Is fixed position");

    if (m_compositingReasons & CompositingReasonPositionSticky)
        jsonArray->pushString("Is sticky position");

    if (m_compositingReasons & CompositingReasonOverflowScrollingTouch)
        jsonArray->pushString("Is a scrollable overflow element");

    if (m_compositingReasons & CompositingReasonAssumedOverlap)
        jsonArray->pushString("Might overlap a composited animation");

    if (m_compositingReasons & CompositingReasonOverlap)
        jsonArray->pushString("Overlaps other composited content");

    if (m_compositingReasons & CompositingReasonNegativeZIndexChildren)
        jsonArray->pushString("Might overlap negative z-index composited content");

    if (m_compositingReasons & CompositingReasonTransformWithCompositedDescendants)
        jsonArray->pushString("Has transform needed by a composited descendant");

    if (m_compositingReasons & CompositingReasonOpacityWithCompositedDescendants)
        jsonArray->pushString("Has opacity needed by a composited descendant");

    if (m_compositingReasons & CompositingReasonMaskWithCompositedDescendants)
        jsonArray->pushString("Has a mask needed by a composited descendant");

    if (m_compositingReasons & CompositingReasonReflectionWithCompositedDescendants)
        jsonArray->pushString("Has a reflection with a composited descendant");

    if (m_compositingReasons & CompositingReasonFilterWithCompositedDescendants)
        jsonArray->pushString("Has filter effect with a composited descendant");

    if (m_compositingReasons & CompositingReasonBlendingWithCompositedDescendants)
        jsonArray->pushString("Has a blend mode with a composited descendant");

    if (m_compositingReasons & CompositingReasonClipsCompositingDescendants)
        jsonArray->pushString("Clips a composited descendant");

    if (m_compositingReasons & CompositingReasonPerspective)
        jsonArray->pushString("Has a perspective transform needed by a composited 3d descendant");

    if (m_compositingReasons & CompositingReasonPreserve3D)
        jsonArray->pushString("Has preserves-3d style with composited 3d descendant");

    if (m_compositingReasons & CompositingReasonReflectionOfCompositedParent)
        jsonArray->pushString("Is the reflection of a composited layer");

    if (m_compositingReasons & CompositingReasonRoot)
        jsonArray->pushString("Is the root");

    if (m_compositingReasons & CompositingReasonLayerForClip)
        jsonArray->pushString("Convenience layer, to clip subtree");

    if (m_compositingReasons & CompositingReasonLayerForScrollbar)
        jsonArray->pushString("Convenience layer for rendering scrollbar");

    if (m_compositingReasons & CompositingReasonLayerForScrollingContainer)
        jsonArray->pushString("Convenience layer, the scrolling container");

    if (m_compositingReasons & CompositingReasonLayerForForeground)
        jsonArray->pushString("Convenience layer, foreground when main layer has negative z-index composited content");

    if (m_compositingReasons & CompositingReasonLayerForBackground)
        jsonArray->pushString("Convenience layer, background when main layer has a composited background");

    if (m_compositingReasons & CompositingReasonLayerForMask)
        jsonArray->pushString("Is a mask layer");

    if (m_compositingReasons & CompositingReasonOverflowScrollingParent)
        jsonArray->pushString("Scroll parent is not an ancestor");

    if (m_compositingReasons & CompositingReasonOutOfFlowClipping)
        jsonArray->pushString("Has clipping ancestor");

    if (m_compositingReasons & CompositingReasonIsolateCompositedDescendants)
        jsonArray->pushString("Should isolate composited descendants");

    jsonObject->setArray("compositing_reasons", jsonArray);
}

void GraphicsLayerDebugInfo::appendDebugName(JSONObject* jsonObject) const
{
    if (m_debugName.isEmpty())
        return;

    jsonObject->setString("layer_name", m_debugName);
}

} // namespace WebCore
