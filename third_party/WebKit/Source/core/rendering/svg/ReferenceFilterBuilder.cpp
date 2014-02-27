/*
 * Copyright (C) 2013 Adobe Systems Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/svg/ReferenceFilterBuilder.h"

#include "SVGNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/dom/Element.h"
#include "core/fetch/DocumentResource.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/graphics/filters/SourceAlpha.h"

namespace WebCore {

HashMap<const FilterOperation*, OwnPtr<DocumentResourceReference> >* ReferenceFilterBuilder::documentResourceReferences = 0;

DocumentResourceReference* ReferenceFilterBuilder::documentResourceReference(const FilterOperation* filterOperation)
{
    if (!documentResourceReferences)
        return 0;

    return documentResourceReferences->get(filterOperation);
}

void ReferenceFilterBuilder::setDocumentResourceReference(const FilterOperation* filterOperation, PassOwnPtr<DocumentResourceReference> documentResourceReference)
{
    if (!documentResourceReferences)
        documentResourceReferences = new HashMap<const FilterOperation*, OwnPtr<DocumentResourceReference> >;
    documentResourceReferences->add(filterOperation, documentResourceReference);
}

void ReferenceFilterBuilder::clearDocumentResourceReference(const FilterOperation* filterOperation)
{
    if (!documentResourceReferences)
        return;

    documentResourceReferences->remove(filterOperation);
}

// Returns whether or not the SVGElement object contains a valid color-interpolation-filters attribute
static bool getSVGElementColorSpace(SVGElement* svgElement, ColorSpace& cs)
{
    if (!svgElement)
        return false;

    const RenderObject* renderer = svgElement->renderer();
    const RenderStyle* style = renderer ? renderer->style() : 0;
    const SVGRenderStyle* svgStyle = style ? style->svgStyle() : 0;
    EColorInterpolation eColorInterpolation = CI_AUTO;
    if (svgStyle) {
        // If a layout has been performed, then we can use the fast path to get this attribute
        eColorInterpolation = svgStyle->colorInterpolationFilters();
    } else {
        // Otherwise, use the slow path by using string comparison (used by external svg files)
        RefPtrWillBeRawPtr<CSSValue> cssValue = svgElement->getPresentationAttribute(AtomicString(SVGNames::color_interpolation_filtersAttr.toString()));
        if (cssValue.get() && cssValue->isPrimitiveValue()) {
            const CSSPrimitiveValue& primitiveValue = *((CSSPrimitiveValue*)cssValue.get());
            eColorInterpolation = (EColorInterpolation)primitiveValue;
        } else {
            return false;
        }
    }

    switch (eColorInterpolation) {
    case CI_AUTO:
    case CI_SRGB:
        cs = ColorSpaceDeviceRGB;
        break;
    case CI_LINEARRGB:
        cs = ColorSpaceLinearRGB;
        break;
    default:
        return false;
    }

    return true;
}

PassRefPtr<FilterEffect> ReferenceFilterBuilder::build(Filter* parentFilter, RenderObject* renderer, FilterEffect* previousEffect, const ReferenceFilterOperation* filterOperation)
{
    if (!renderer)
        return nullptr;

    Document* document = &renderer->document();

    if (DocumentResourceReference* documentResourceRef = documentResourceReference(filterOperation)) {
        DocumentResource* cachedSVGDocument = documentResourceRef->document();

        // If we have an SVG document, this is an external reference. Otherwise
        // we look up the referenced node in the current document.
        if (cachedSVGDocument)
            document = cachedSVGDocument->document();
    }

    if (!document)
        return nullptr;

    Element* filter = document->getElementById(filterOperation->fragment());

    if (!filter) {
        // Although we did not find the referenced filter, it might exist later
        // in the document
        document->accessSVGExtensions().addPendingResource(filterOperation->fragment(), toElement(renderer->node()));
        return nullptr;
    }

    if (!filter->isSVGElement() || !filter->hasTagName(SVGNames::filterTag))
        return nullptr;

    SVGFilterElement* filterElement = toSVGFilterElement(toSVGElement(filter));

    // FIXME: Figure out what to do with SourceAlpha. Right now, we're
    // using the alpha of the original input layer, which is obviously
    // wrong. We should probably be extracting the alpha from the
    // previousEffect, but this requires some more processing.
    // This may need a spec clarification.
    RefPtr<SVGFilterBuilder> builder = SVGFilterBuilder::create(previousEffect, SourceAlpha::create(parentFilter));

    ColorSpace filterColorSpace = ColorSpaceDeviceRGB;
    bool useFilterColorSpace = getSVGElementColorSpace(filterElement, filterColorSpace);

    for (Element* child = ElementTraversal::firstWithin(*filterElement); child; child = ElementTraversal::nextSibling(*child)) {
        if (!child->isSVGElement())
            continue;

        SVGElement* element = toSVGElement(child);
        if (!element->isFilterEffect())
            continue;

        SVGFilterPrimitiveStandardAttributes* effectElement = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);

        RefPtr<FilterEffect> effect = effectElement->build(builder.get(), parentFilter);
        if (!effect)
            continue;

        effectElement->setStandardAttributes(effect.get());
        effect->setEffectBoundaries(SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(effectElement, filterElement->primitiveUnits()->currentValue()->enumValue(), parentFilter->sourceImageRect()));
        ColorSpace colorSpace = filterColorSpace;
        if (useFilterColorSpace || getSVGElementColorSpace(effectElement, colorSpace))
            effect->setOperatingColorSpace(colorSpace);
        builder->add(AtomicString(effectElement->result()->currentValue()->value()), effect);
    }
    return builder->lastEffect();
}

} // namespace WebCore
