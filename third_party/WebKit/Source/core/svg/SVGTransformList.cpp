/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "core/svg/SVGTransformList.h"

#include "core/platform/graphics/transforms/AffineTransform.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/SVGTransformable.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

SVGTransform SVGTransformList::createSVGTransformFromMatrix(const SVGMatrix& matrix) const
{
    return SVGSVGElement::createSVGTransformFromMatrix(matrix);
}

SVGTransform SVGTransformList::consolidate()
{
    AffineTransform matrix;
    if (!concatenate(matrix))
        return SVGTransform();

    SVGTransform transform(matrix);
    clear();
    append(transform);
    return transform;
}

bool SVGTransformList::concatenate(AffineTransform& result) const
{
    unsigned size = this->size();
    if (!size)
        return false;

    for (unsigned i = 0; i < size; ++i)
        result *= at(i).matrix();

    return true;
}

String SVGTransformList::valueAsString() const
{
    StringBuilder builder;
    unsigned size = this->size();
    for (unsigned i = 0; i < size; ++i) {
        if (i > 0)
            builder.append(' ');

        builder.append(at(i).valueAsString());
    }

    return builder.toString();
}

void SVGTransformList::parse(const String& transform)
{
    if (transform.isEmpty()) {
        // FIXME: The parseTransformAttribute function secretly calls clear()
        // based on a |mode| parameter. We should study whether we should
        // remove the |mode| parameter and force callers to call clear()
        // themselves.
        clear();
    } else if (transform.is8Bit()) {
        const LChar* ptr = transform.characters8();
        const LChar* end = ptr + transform.length();
        if (!SVGTransformable::parseTransformAttribute(*this, ptr, end))
            clear();
    } else {
        const UChar* ptr = transform.characters16();
        const UChar* end = ptr + transform.length();
        if (!SVGTransformable::parseTransformAttribute(*this, ptr, end))
            clear();
    }
}

} // namespace WebCore
