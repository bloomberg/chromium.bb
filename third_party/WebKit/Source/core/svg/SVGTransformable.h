/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGTransformable_h
#define SVGTransformable_h

#include "core/svg/SVGLocatable.h"
#include "core/svg/SVGTransform.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class AffineTransform;
class SVGTransformList;

class SVGTransformable : virtual public SVGLocatable {
public:
    enum TransformParsingMode {
        ClearList,
        DoNotClearList
    };

    virtual ~SVGTransformable();

    static bool parseTransformAttribute(SVGTransformList&, const LChar*& ptr, const LChar* end, TransformParsingMode = ClearList);
    static bool parseTransformAttribute(SVGTransformList&, const UChar*& ptr, const UChar* end, TransformParsingMode = ClearList);

    static bool parseTransformValue(unsigned type, const LChar*& ptr, const LChar* end, SVGTransform&);
    static bool parseTransformValue(unsigned type, const UChar*& ptr, const UChar* end, SVGTransform&);

    static SVGTransform::SVGTransformType parseTransformType(const String&);

    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const { return animatedLocalTransform(); }
    virtual AffineTransform animatedLocalTransform() const = 0;

private:
    template<typename CharType>
    static bool parseTransformAttributeInternal(SVGTransformList&, const CharType*& ptr, const CharType* end, TransformParsingMode);
};

} // namespace WebCore

#endif // SVGTransformable_h
