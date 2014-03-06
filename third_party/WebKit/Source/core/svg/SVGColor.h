/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGColor_h
#define SVGColor_h

#include "core/css/CSSValue.h"
#include "core/css/StyleColor.h"
#include "platform/graphics/Color.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class ExceptionState;
class RGBColor;

class SVGColor : public CSSValue {
public:
    enum SVGColorType {
        SVG_COLORTYPE_UNKNOWN = 0,
        SVG_COLORTYPE_RGBCOLOR = 1,
        SVG_COLORTYPE_RGBCOLOR_ICCCOLOR = 2,
        SVG_COLORTYPE_CURRENTCOLOR = 3
    };

    static PassRefPtrWillBeRawPtr<SVGColor> createFromString(const String& rgbColor)
    {
        RefPtrWillBeRawPtr<SVGColor> color = adoptRefWillBeRefCountedGarbageCollected(new SVGColor(SVG_COLORTYPE_RGBCOLOR));
        StyleColor styleColor = colorFromRGBColorString(rgbColor);
        ASSERT(!styleColor.isCurrentColor());
        color->setColor(styleColor.color());
        return color.release();
    }

    static PassRefPtrWillBeRawPtr<SVGColor> createFromColor(const Color& rgbColor)
    {
        RefPtrWillBeRawPtr<SVGColor> color = adoptRefWillBeRefCountedGarbageCollected(new SVGColor(SVG_COLORTYPE_RGBCOLOR));
        color->setColor(rgbColor);
        return color.release();
    }

    static PassRefPtrWillBeRawPtr<SVGColor> createCurrentColor()
    {
        return adoptRefWillBeRefCountedGarbageCollected(new SVGColor(SVG_COLORTYPE_CURRENTCOLOR));
    }

    const Color& color() const { return m_color; }
    const SVGColorType& colorType() const { return m_colorType; }

    static StyleColor colorFromRGBColorString(const String&);

    String customCSSText() const;

    ~SVGColor() { }

    PassRefPtrWillBeRawPtr<SVGColor> cloneForCSSOM() const;

    bool equals(const SVGColor&) const;

    void traceAfterDispatch(Visitor* visitor) { CSSValue::traceAfterDispatch(visitor); }

protected:
    friend class CSSComputedStyleDeclaration;

    SVGColor(ClassType, const SVGColorType&);
    SVGColor(ClassType, const SVGColor& cloneFrom);

    void setColor(const Color& color)
    {
        m_color = color;
        setColorType(SVG_COLORTYPE_RGBCOLOR);
    }
    void setColorType(const SVGColorType& type) { m_colorType = type; }

private:
    SVGColor(const SVGColorType&);

    Color m_color;
    SVGColorType m_colorType;
};

DEFINE_CSS_VALUE_TYPE_CASTS(SVGColor, isSVGColor());

} // namespace WebCore

#endif // SVGColor_h
