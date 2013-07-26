/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef ShadowData_h
#define ShadowData_h

#include "core/css/StyleColor.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/LayoutRect.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

enum ShadowStyle { Normal, Inset };

// This class holds information about shadows for the text-shadow and box-shadow properties.
class ShadowData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ShadowData> create() { return adoptPtr(new ShadowData); }
    static PassOwnPtr<ShadowData> create(const IntPoint& location, int blur, int spread, ShadowStyle style, const StyleColor& color)
    {
        return adoptPtr(new ShadowData(location, blur, spread, style, color));
    }
    // This clones the whole ShadowData linked list.
    PassOwnPtr<ShadowData> clone() const
    {
        return adoptPtr(new ShadowData(*this));
    }

    bool operator==(const ShadowData&) const;
    bool operator!=(const ShadowData& o) const { return !(*this == o); }

    int x() const { return m_location.x(); }
    int y() const { return m_location.y(); }
    IntPoint location() const { return m_location; }
    int blur() const { return m_blur; }
    int spread() const { return m_spread; }
    ShadowStyle style() const { return m_style; }
    const StyleColor& color() const { return m_color; }

    const ShadowData* next() const { return m_next.get(); }
    void setNext(PassOwnPtr<ShadowData> shadow) { m_next = shadow; }

    void adjustRectForShadow(LayoutRect&, int additionalOutlineSize = 0) const;
    void adjustRectForShadow(FloatRect&, int additionalOutlineSize = 0) const;

private:
    ShadowData()
        : m_blur(0)
        , m_spread(0)
        , m_style(Normal)
    {
    }

    ShadowData(const IntPoint& location, int blur, int spread, ShadowStyle style, const StyleColor& color)
        : m_location(location)
        , m_blur(blur)
        , m_spread(spread)
        , m_color(color)
        , m_style(style)
    {
    }

    ShadowData(const ShadowData&);

    IntPoint m_location;
    int m_blur;
    int m_spread;
    StyleColor m_color;
    ShadowStyle m_style;
    OwnPtr<ShadowData> m_next;
};

// Helper method to handle nullptr, otherwise all callers need an ugly ternary.
inline PassOwnPtr<ShadowData> cloneShadow(const ShadowData* shadow)
{
    return shadow ? shadow->clone() : nullptr;
}

inline PassOwnPtr<ShadowData> cloneShadow(const OwnPtr<ShadowData>& shadow)
{
    return cloneShadow(shadow.get());
}

} // namespace WebCore

#endif // ShadowData_h
