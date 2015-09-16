/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef Filter_h
#define Filter_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"

namespace blink {

class SourceGraphic;
class FilterEffect;

class PLATFORM_EXPORT Filter : public RefCountedWillBeGarbageCollectedFinalized<Filter> {
public:
    static PassRefPtrWillBeRawPtr<Filter> create(const FloatRect& referenceBox, const FloatRect& filterRegion, float scale)
    {
        return adoptRefWillBeNoop(new Filter(referenceBox, filterRegion, scale));
    }

    static PassRefPtrWillBeRawPtr<Filter> create(float scale)
    {
        return adoptRefWillBeNoop(new Filter(FloatRect(), FloatRect(), scale));
    }

    virtual ~Filter();
    DECLARE_VIRTUAL_TRACE();

    float scale() const { return m_scale; }
    FloatRect mapLocalRectToAbsoluteRect(const FloatRect&) const;
    FloatRect mapAbsoluteRectToLocalRect(const FloatRect&) const;

    virtual float applyHorizontalScale(float value) const { return m_scale * value; }
    virtual float applyVerticalScale(float value) const { return m_scale * value; }

    virtual FloatPoint3D resolve3dPoint(const FloatPoint3D& point) const { return point; }

    FloatRect absoluteFilterRegion() const { return mapLocalRectToAbsoluteRect(m_filterRegion); }

    const FloatRect& filterRegion() const { return m_filterRegion; }
    const FloatRect& referenceBox() const { return m_referenceBox; }

    void setLastEffect(PassRefPtrWillBeRawPtr<FilterEffect>);
    FilterEffect* lastEffect() const { return m_lastEffect.get(); }

    SourceGraphic* sourceGraphic() const { return m_sourceGraphic.get(); }

protected:
    Filter(const FloatRect& referenceBox, const FloatRect& filterRegion, float scale);

private:
    FloatRect m_referenceBox;
    FloatRect m_filterRegion;
    float m_scale;

    RefPtrWillBeMember<SourceGraphic> m_sourceGraphic;
    RefPtrWillBeMember<FilterEffect> m_lastEffect;
};

} // namespace blink

#endif // Filter_h
