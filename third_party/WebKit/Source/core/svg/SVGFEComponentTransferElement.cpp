/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/svg/SVGFEComponentTransferElement.h"

#include "SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/SVGFEFuncAElement.h"
#include "core/svg/SVGFEFuncBElement.h"
#include "core/svg/SVGFEFuncGElement.h"
#include "core/svg/SVGFEFuncRElement.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

// Animated property definitions

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEComponentTransferElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEComponentTransferElement::SVGFEComponentTransferElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feComponentTransferTag, document)
    , m_in1(SVGAnimatedString::create(this, SVGNames::inAttr, SVGString::create()))
{
    ScriptWrappable::init(this);
    addToPropertyMap(m_in1);
    registerAnimatedPropertiesForSVGFEComponentTransferElement();
}

PassRefPtr<SVGFEComponentTransferElement> SVGFEComponentTransferElement::create(Document& document)
{
    return adoptRef(new SVGFEComponentTransferElement(document));
}

bool SVGFEComponentTransferElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        supportedAttributes.add(SVGNames::inAttr);
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEComponentTransferElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::inAttr)
        m_in1->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

PassRefPtr<FilterEffect> SVGFEComponentTransferElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(m_in1->currentValue()->value()));

    if (!input1)
        return nullptr;

    ComponentTransferFunction red;
    ComponentTransferFunction green;
    ComponentTransferFunction blue;
    ComponentTransferFunction alpha;

    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (node->hasTagName(SVGNames::feFuncRTag))
            red = toSVGFEFuncRElement(node)->transferFunction();
        else if (node->hasTagName(SVGNames::feFuncGTag))
            green = toSVGFEFuncGElement(node)->transferFunction();
        else if (node->hasTagName(SVGNames::feFuncBTag))
            blue = toSVGFEFuncBElement(node)->transferFunction();
        else if (node->hasTagName(SVGNames::feFuncATag))
            alpha = toSVGFEFuncAElement(node)->transferFunction();
    }

    RefPtr<FilterEffect> effect = FEComponentTransfer::create(filter, red, green, blue, alpha);
    effect->inputEffects().append(input1);
    return effect.release();
}

}
