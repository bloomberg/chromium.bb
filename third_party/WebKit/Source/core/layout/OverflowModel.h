/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef OverflowModel_h
#define OverflowModel_h

#include "platform/geometry/LayoutRect.h"

namespace blink {

// OverflowModel is a class for tracking content that spills out of an object.
// This class is used by LayoutBox and InlineFlowBox.
//
// There are 3 types of overflows that we store: layout, visual overflow and
// content visual overflows.
// All overflows are in logical coordinates.
//
// This class models the overflows as rectangles that unite all the sources of
// overflow. This is the natural choice for layout overflow (scrollbars are
// linear in nature, thus are modelled by rectangles in 2D). For visual overflow
// and content visual overflow, this is a first order simplification though as
// they can be thought of as a collection of (potentially overlapping)
// rectangles.
//
// Layout overflow is the overflow that is reachable via scrollbars. It is used
// to size the scrollbar thumb and determine its position, which is determined
// by the maximum layout overflow size.
// Layout overflow cannot occur without an overflow clip as this is the only way
// to get scrollbars. As its name implies, it is a direct consequence of layout.
// Example of layout overflow:
// * in the inline case, a tall image could spill out of a line box.
// * 'height' / 'width' set to a value smaller than the one needed by the
//   descendants.
// Due to how scrollbars work, no overflow in the logical top and logical left
// direction is allowed(see LayoutBox::addLayoutOverflow).
//
// Visual overflow covers all the effects that visually bleed out of the box.
// Its primary use is to determine the area to invalidate.
// Visual overflow includes shadows ('text-shadow' / 'box-shadow'), text stroke,
// 'outline' and 'border-image'.
//
// Content visual overflow includes anything that would bleed out of the box and
// would be clipped by the overflow clip ('overflow' != visible). This
// corresponds to children that overflows their parent.
// It's important to note that this overflow ignores descendants with
// self-painting layers (see the SELF-PAINTING LAYER section in PaintLayer).
// This is required by the simplification made by this model (single united
// rectangle) to avoid gigantic invalidation. A good example for this is
// positioned objects that can be anywhere on the page and could artificially
// inflate the visual overflow.
// The main use of content visual overflow is to prevent unneeded clipping in
// BoxPainter (see https://crbug.com/238732). Note that the code path for
// self-painting layer is handled by PaintLayerPainter, which relies on
// PaintLayerClipper and thus ignores this optimization.
//
// This object is allocated only when some of these fields have non-default
// values in the owning box. Care should be taken to use LayoutBox adder
// functions (addLayoutOverflow, addVisualOverflow, addContentsVisualOverflow)
// to keep this invariant.
class OverflowModel {
    WTF_MAKE_NONCOPYABLE(OverflowModel);
    USING_FAST_MALLOC(OverflowModel);
public:
    OverflowModel(const LayoutRect& layoutRect, const LayoutRect& visualRect)
        : m_layoutOverflow(layoutRect)
        , m_visualOverflow(visualRect)
    {
    }

    const LayoutRect layoutOverflowRect() const { return m_layoutOverflow; }
    const LayoutRect visualOverflowRect() const { return m_visualOverflow; }
    LayoutRect contentsVisualOverflowRect() const { return m_contentsVisualOverflow; }

    void move(LayoutUnit dx, LayoutUnit dy);

    void addLayoutOverflow(const LayoutRect&);
    void addVisualOverflow(const LayoutRect&);
    void addContentsVisualOverflow(const LayoutRect& rect) { m_contentsVisualOverflow.unite(rect); }

    void setLayoutOverflow(const LayoutRect&);
    void setVisualOverflow(const LayoutRect&);

    LayoutUnit layoutClientAfterEdge() const { return m_layoutClientAfterEdge; }
    void setLayoutClientAfterEdge(LayoutUnit clientAfterEdge) { m_layoutClientAfterEdge = clientAfterEdge; }

private:
    // The layout overflow rectangle. See class description for details.
    LayoutRect m_layoutOverflow;

    // The visual overflow rectangle. See class description for details.
    LayoutRect m_visualOverflow;

    // The content visual overflow rectangle. See class description for details.
    LayoutRect m_contentsVisualOverflow;

    LayoutUnit m_layoutClientAfterEdge;
};

inline void OverflowModel::move(LayoutUnit dx, LayoutUnit dy)
{
    m_layoutOverflow.move(dx, dy);
    m_visualOverflow.move(dx, dy);
    m_contentsVisualOverflow.move(dx, dy);
}

inline void OverflowModel::addLayoutOverflow(const LayoutRect& rect)
{
    LayoutUnit maxX = std::max(rect.maxX(), m_layoutOverflow.maxX());
    LayoutUnit maxY = std::max(rect.maxY(), m_layoutOverflow.maxY());
    LayoutUnit minX = std::min(rect.x(), m_layoutOverflow.x());
    LayoutUnit minY = std::min(rect.y(), m_layoutOverflow.y());
    // In case the width/height is larger than LayoutUnit can represent, fix the right/bottom edge and shift the top/left ones
    m_layoutOverflow.setWidth(maxX - minX);
    m_layoutOverflow.setHeight(maxY - minY);
    m_layoutOverflow.setX(maxX - m_layoutOverflow.width());
    m_layoutOverflow.setY(maxY - m_layoutOverflow.height());
}

inline void OverflowModel::addVisualOverflow(const LayoutRect& rect)
{
    LayoutUnit maxX = std::max(rect.maxX(), m_visualOverflow.maxX());
    LayoutUnit maxY = std::max(rect.maxY(), m_visualOverflow.maxY());
    m_visualOverflow.setX(std::min(rect.x(), m_visualOverflow.x()));
    m_visualOverflow.setY(std::min(rect.y(), m_visualOverflow.y()));
    m_visualOverflow.setWidth(maxX - m_visualOverflow.x());
    m_visualOverflow.setHeight(maxY - m_visualOverflow.y());
}

inline void OverflowModel::setLayoutOverflow(const LayoutRect& rect)
{
    m_layoutOverflow = rect;
}

inline void OverflowModel::setVisualOverflow(const LayoutRect& rect)
{
    m_visualOverflow = rect;
}

} // namespace blink

#endif // OverflowModel_h
