/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderListItem_h
#define RenderListItem_h

#include "core/rendering/RenderBlockFlow.h"

namespace blink {

class HTMLOListElement;
class RenderListMarker;

class RenderListItem final : public RenderBlockFlow {
public:
    explicit RenderListItem(Element*);
    virtual void trace(Visitor*) override;

    int value() const { if (!m_isValueUpToDate) updateValueNow(); return m_value; }
    void updateValue();

    bool hasExplicitValue() const { return m_hasExplicitValue; }
    int explicitValue() const { return m_explicitValue; }
    void setExplicitValue(int value);
    void clearExplicitValue();

    void setNotInList(bool);
    bool notInList() const { return m_notInList; }

    const String& markerText() const;

    void updateListMarkerNumbers();

    static void updateItemValuesForOrderedList(const HTMLOListElement*);
    static unsigned itemCountForOrderedList(const HTMLOListElement*);

    bool isEmpty() const;

private:
    virtual const char* renderName() const override { return "RenderListItem"; }

    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectListItem || RenderBlockFlow::isOfType(type); }

    virtual void willBeDestroyed() override;

    virtual void insertedIntoTree() override;
    virtual void willBeRemovedFromTree() override;

    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    virtual void layout() override;

    // Returns true if we re-attached and updated the location of the marker.
    bool updateMarkerLocation();
    void updateMarkerLocationAndInvalidateWidth();

    void positionListMarker();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual void addOverflowFromChildren() override;

    inline int calcValue() const;
    void updateValueNow() const;
    void explicitValueChanged();

    int m_explicitValue;
    RawPtrWillBeMember<RenderListMarker> m_marker;
    mutable int m_value;

    bool m_hasExplicitValue : 1;
    mutable bool m_isValueUpToDate : 1;
    bool m_notInList : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderListItem, isListItem());

} // namespace blink

#endif // RenderListItem_h
