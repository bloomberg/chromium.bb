/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGAltGlyphElement.h"

#include "SVGNames.h"
#include "XLinkNames.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/rendering/svg/RenderSVGTSpan.h"
#include "core/svg/SVGAltGlyphDefElement.h"

namespace WebCore {

// Animated property definitions

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGAltGlyphElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTextPositioningElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGAltGlyphElement::SVGAltGlyphElement(Document& document)
    : SVGTextPositioningElement(SVGNames::altGlyphTag, document)
    , SVGURIReference(this)
{
    ScriptWrappable::init(this);
}

PassRefPtr<SVGAltGlyphElement> SVGAltGlyphElement::create(Document& document)
{
    return adoptRef(new SVGAltGlyphElement(document));
}

void SVGAltGlyphElement::setGlyphRef(const AtomicString&, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NoModificationAllowedError, ExceptionMessages::readOnly());
}

const AtomicString& SVGAltGlyphElement::glyphRef() const
{
    return fastGetAttribute(SVGNames::glyphRefAttr);
}

void SVGAltGlyphElement::setFormat(const AtomicString&, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NoModificationAllowedError, ExceptionMessages::readOnly());
}

const AtomicString& SVGAltGlyphElement::format() const
{
    return fastGetAttribute(SVGNames::formatAttr);
}

RenderObject* SVGAltGlyphElement::createRenderer(RenderStyle*)
{
    return new RenderSVGTSpan(this);
}

bool SVGAltGlyphElement::hasValidGlyphElements(Vector<AtomicString>& glyphNames) const
{
    AtomicString target;
    Element* element = targetElementFromIRIString(getAttribute(XLinkNames::hrefAttr), document(), &target);
    if (!element)
        return false;

    if (element->hasTagName(SVGNames::glyphTag)) {
        glyphNames.append(target);
        return true;
    }

    if (element->hasTagName(SVGNames::altGlyphDefTag)
        && toSVGAltGlyphDefElement(element)->hasValidGlyphElements(glyphNames))
        return true;

    return false;
}

}

#endif
