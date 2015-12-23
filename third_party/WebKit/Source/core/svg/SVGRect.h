/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGRect_h
#define SVGRect_h

#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyHelper.h"
#include "platform/geometry/FloatRect.h"
#include "wtf/Allocator.h"

namespace blink {

class SVGRectTearOff;

class SVGRect : public SVGPropertyHelper<SVGRect> {
public:
    typedef SVGRectTearOff TearOffType;

    static PassRefPtrWillBeRawPtr<SVGRect> create()
    {
        return adoptRefWillBeNoop(new SVGRect());
    }

    static PassRefPtrWillBeRawPtr<SVGRect> createInvalid()
    {
        RefPtrWillBeRawPtr<SVGRect> rect = adoptRefWillBeNoop(new SVGRect());
        rect->setInvalid();
        return rect.release();
    }

    static PassRefPtrWillBeRawPtr<SVGRect> create(const FloatRect& rect)
    {
        return adoptRefWillBeNoop(new SVGRect(rect));
    }

    PassRefPtrWillBeRawPtr<SVGRect> clone() const;

    const FloatRect& value() const { return m_value; }
    void setValue(const FloatRect& v) { m_value = v; }

    float x() const { return m_value.x(); }
    float y() const { return m_value.y(); }
    float width() const { return m_value.width(); }
    float height() const { return m_value.height(); }
    void setX(float f) { m_value.setX(f); }
    void setY(float f) { m_value.setY(f); }
    void setWidth(float f) { m_value.setWidth(f); }
    void setHeight(float f) { m_value.setHeight(f); }

    String valueAsString() const override;
    SVGParsingError setValueAsString(const String&);

    void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) override;
    void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtrWillBeRawPtr<SVGPropertyBase> from, PassRefPtrWillBeRawPtr<SVGPropertyBase> to, PassRefPtrWillBeRawPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) override;
    float calculateDistance(PassRefPtrWillBeRawPtr<SVGPropertyBase> to, SVGElement* contextElement) override;

    bool isValid() const { return m_isValid; }
    void setInvalid();

    static AnimatedPropertyType classType() { return AnimatedRect; }

private:
    SVGRect();
    SVGRect(const FloatRect&);

    template<typename CharType>
    bool parse(const CharType*& ptr, const CharType* end);

    bool m_isValid;
    FloatRect m_value;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGRect);

} // namespace blink

#endif // SVGRect_h
