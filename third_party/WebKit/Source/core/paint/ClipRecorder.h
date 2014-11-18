// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "core/paint/ViewDisplayList.h"
#include "core/rendering/LayerPaintingInfo.h"
#include "core/rendering/PaintPhase.h"
#include "core/rendering/RenderLayerModelObject.h"
#include "platform/geometry/RoundedRect.h"
#include "wtf/Vector.h"

namespace blink {

class ClipRect;
class GraphicsContext;

class ClipDisplayItem : public DisplayItem {
public:
    ClipDisplayItem(const RenderLayerModelObject* renderer, Type type, IntRect clipRect)
        : DisplayItem(renderer, type), m_clipRect(clipRect) { }

    Vector<RoundedRect>& roundedRectClips() { return m_roundedRectClips; }
    virtual void replay(GraphicsContext*) override;

    IntRect m_clipRect;
    Vector<RoundedRect> m_roundedRectClips;
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

class EndClipDisplayItem : public DisplayItem {
public:
    EndClipDisplayItem(const RenderLayerModelObject* renderer) : DisplayItem(renderer, EndClip) { }

private:
    virtual void replay(GraphicsContext*) override;
};

class ClipRecorder {
public:

    enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };

    // Set rounded clip rectangles defined by border radii all the way from the LayerPaintingInfo
    // "root" layer down to the specified layer (or the parent of said layer, in case
    // BorderRadiusClippingRule says to skip self). fragmentOffset is used for multicol, to specify
    // the translation required to get from flow thread coordinates to visual coordinates for a
    // certain column.
    // FIXME: The BorderRadiusClippingRule parameter is really useless now. If we want to skip self,
    // why not just supply the parent layer as the first parameter instead?
    explicit ClipRecorder(const RenderLayerModelObject*, GraphicsContext*, DisplayItem::Type, const ClipRect&, const LayerPaintingInfo* localPaintingInfo, const LayoutPoint& fragmentOffset, PaintLayerFlags, BorderRadiusClippingRule = IncludeSelfForBorderRadius);

    ~ClipRecorder();

private:

    void collectRoundedRectClips(RenderLayer&, const LayerPaintingInfo& localPaintingInfo, GraphicsContext*, const LayoutPoint& fragmentOffset, PaintLayerFlags,
        BorderRadiusClippingRule, Vector<RoundedRect>& roundedRectClips);

    GraphicsContext* m_graphicsContext;
    const RenderLayerModelObject* m_renderer;
};

} // namespace blink

#endif // ViewDisplayList_h
