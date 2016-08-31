/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutFieldset_h
#define LayoutFieldset_h

#include "core/layout/LayoutFlexibleBox.h"

namespace blink {

class LayoutFieldset final : public LayoutFlexibleBox {
public:
    explicit LayoutFieldset(Element*);

    LayoutBox* findInFlowLegend() const;

    const char* name() const override { return "LayoutFieldset"; }

private:
    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;
    bool avoidsFloats() const override { return true; }

    // We override the two baseline functions because we want our baseline to be the bottom of our margin box.
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode) const override;
    int inlineBlockBaseline(LineDirectionMode) const override { return -1; }

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    bool createsAnonymousWrapper() const override { return true; }
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectFieldset || LayoutFlexibleBox::isOfType(type); }
    LayoutObject* layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope&) override;
    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&) const override;
    void paintMask(const PaintInfo&, const LayoutPoint&) const override;
    void updateAnonymousChildStyle(const LayoutObject& child, ComputedStyle& childStyle) const override;

    void createInnerBlock();
    void setLogicalLeftForChild(LayoutBox& child, LayoutUnit logicalLeft);
    void setLogicalTopForChild(LayoutBox& child, LayoutUnit logicalTop);
    void removeChild(LayoutObject*) override;

    LayoutBlock* m_innerBlock;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFieldset, isFieldset());

} // namespace blink

#endif // LayoutFieldset_h
