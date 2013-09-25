/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
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
#include "core/svg/SVGViewSpec.h"

#include "SVGNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTransformable.h"

namespace WebCore {

// Define custom animated property 'viewBox'.
const SVGPropertyInfo* SVGViewSpec::viewBoxPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedRect,
                                             PropertyIsReadOnly,
                                             SVGNames::viewBoxAttr,
                                             viewBoxIdentifier(),
                                             0,
                                             0);
    }
    return s_propertyInfo;
}

// Define custom animated property 'preserveAspectRatio'.
const SVGPropertyInfo* SVGViewSpec::preserveAspectRatioPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedPreserveAspectRatio,
                                             PropertyIsReadOnly,
                                             SVGNames::preserveAspectRatioAttr,
                                             preserveAspectRatioIdentifier(),
                                             0,
                                             0);
    }
    return s_propertyInfo;
}


// Define custom non-animated property 'transform'.
const SVGPropertyInfo* SVGViewSpec::transformPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedTransformList,
                                             PropertyIsReadOnly,
                                             SVGNames::transformAttr,
                                             transformIdentifier(),
                                             0,
                                             0);
    }
    return s_propertyInfo;
}

SVGViewSpec::SVGViewSpec(SVGElement* contextElement)
    : m_contextElement(contextElement)
    , m_zoomAndPan(SVGZoomAndPanMagnify)
{
    ASSERT(m_contextElement);
    ScriptWrappable::init(this);
}

const AtomicString& SVGViewSpec::viewBoxIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGViewSpecViewBoxAttribute", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

const AtomicString& SVGViewSpec::preserveAspectRatioIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGViewSpecPreserveAspectRatioAttribute", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

const AtomicString& SVGViewSpec::transformIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGViewSpecTransformAttribute", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

void SVGViewSpec::setZoomAndPan(unsigned short, ExceptionState& es)
{
    // SVGViewSpec and all of its content is read-only.
    es.throwUninformativeAndGenericDOMException(NoModificationAllowedError);
}

void SVGViewSpec::setTransformString(const String& transform)
{
    if (!m_contextElement)
        return;

    SVGTransformList newList;
    newList.parse(transform);

    if (SVGAnimatedProperty* wrapper = SVGAnimatedProperty::lookupWrapper<SVGElement, SVGAnimatedTransformList>(m_contextElement, transformPropertyInfo()))
        static_cast<SVGAnimatedTransformList*>(wrapper)->detachListWrappers(newList.size());

    m_transform = newList;
}

String SVGViewSpec::transformString() const
{
    return SVGPropertyTraits<SVGTransformList>::toString(m_transform);
}

String SVGViewSpec::viewBoxString() const
{
    return SVGPropertyTraits<FloatRect>::toString(viewBoxBaseValue());
}

String SVGViewSpec::preserveAspectRatioString() const
{
    return SVGPropertyTraits<SVGPreserveAspectRatio>::toString(preserveAspectRatioBaseValue());
}

SVGElement* SVGViewSpec::viewTarget() const
{
    if (!m_contextElement)
        return 0;
    Element* element = m_contextElement->treeScope().getElementById(m_viewTargetString);
    if (!element || !element->isSVGElement())
        return 0;
    return toSVGElement(element);
}

SVGTransformListPropertyTearOff* SVGViewSpec::transform()
{
    if (!m_contextElement)
        return 0;
    // Return the animVal here, as its readonly by default - which is exactly what we want here.
    return static_cast<SVGTransformListPropertyTearOff*>(static_pointer_cast<SVGAnimatedTransformList>(lookupOrCreateTransformWrapper(this))->animVal());
}

PassRefPtr<SVGAnimatedRect> SVGViewSpec::viewBox()
{
    if (!m_contextElement)
        return 0;
    return static_pointer_cast<SVGAnimatedRect>(lookupOrCreateViewBoxWrapper(this));
}

PassRefPtr<SVGAnimatedPreserveAspectRatio> SVGViewSpec::preserveAspectRatio()
{
    if (!m_contextElement)
        return 0;
    return static_pointer_cast<SVGAnimatedPreserveAspectRatio>(lookupOrCreatePreserveAspectRatioWrapper(this));
}

PassRefPtr<SVGAnimatedProperty> SVGViewSpec::lookupOrCreateViewBoxWrapper(SVGViewSpec* ownerType)
{
    ASSERT(ownerType);
    ASSERT(ownerType->contextElement());
    return SVGAnimatedProperty::lookupOrCreateWrapper<SVGElement, SVGAnimatedRect, FloatRect>(ownerType->contextElement(), viewBoxPropertyInfo(), ownerType->m_viewBox);
}

PassRefPtr<SVGAnimatedProperty> SVGViewSpec::lookupOrCreatePreserveAspectRatioWrapper(SVGViewSpec* ownerType)
{
    ASSERT(ownerType);
    ASSERT(ownerType->contextElement());
    return SVGAnimatedProperty::lookupOrCreateWrapper<SVGElement, SVGAnimatedPreserveAspectRatio, SVGPreserveAspectRatio>(ownerType->contextElement(), preserveAspectRatioPropertyInfo(), ownerType->m_preserveAspectRatio);
}

PassRefPtr<SVGAnimatedProperty> SVGViewSpec::lookupOrCreateTransformWrapper(SVGViewSpec* ownerType)
{
    ASSERT(ownerType);
    ASSERT(ownerType->contextElement());
    return SVGAnimatedProperty::lookupOrCreateWrapper<SVGElement, SVGAnimatedTransformList, SVGTransformList>(ownerType->contextElement(), transformPropertyInfo(), ownerType->m_transform);
}

void SVGViewSpec::reset()
{
    m_zoomAndPan = SVGZoomAndPanMagnify;
    m_transform.clear();
    m_viewBox = FloatRect();
    m_preserveAspectRatio = SVGPreserveAspectRatio();
    m_viewTargetString = emptyString();
}

static const LChar svgViewSpec[] = {'s', 'v', 'g', 'V', 'i', 'e', 'w'};
static const LChar viewBoxSpec[] = {'v', 'i', 'e', 'w', 'B', 'o', 'x'};
static const LChar preserveAspectRatioSpec[] = {'p', 'r', 'e', 's', 'e', 'r', 'v', 'e', 'A', 's', 'p', 'e', 'c', 't', 'R', 'a', 't', 'i', 'o'};
static const LChar transformSpec[] = {'t', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm'};
static const LChar zoomAndPanSpec[] = {'z', 'o', 'o', 'm', 'A', 'n', 'd', 'P', 'a', 'n'};
static const LChar viewTargetSpec[] =  {'v', 'i', 'e', 'w', 'T', 'a', 'r', 'g', 'e', 't'};

template<typename CharType>
bool SVGViewSpec::parseViewSpecInternal(const CharType* ptr, const CharType* end)
{
    if (!skipString(ptr, end, svgViewSpec, WTF_ARRAY_LENGTH(svgViewSpec)))
        return false;

    if (ptr >= end || *ptr != '(')
        return false;
    ptr++;

    while (ptr < end && *ptr != ')') {
        if (*ptr == 'v') {
            if (skipString(ptr, end, viewBoxSpec, WTF_ARRAY_LENGTH(viewBoxSpec))) {
                if (ptr >= end || *ptr != '(')
                    return false;
                ptr++;
                FloatRect viewBox;
                if (!SVGFitToViewBox::parseViewBox(&m_contextElement->document(), ptr, end, viewBox, false))
                    return false;
                setViewBoxBaseValue(viewBox);
                if (ptr >= end || *ptr != ')')
                    return false;
                ptr++;
            } else if (skipString(ptr, end, viewTargetSpec, WTF_ARRAY_LENGTH(viewTargetSpec))) {
                if (ptr >= end || *ptr != '(')
                    return false;
                const CharType* viewTargetStart = ++ptr;
                while (ptr < end && *ptr != ')')
                    ptr++;
                if (ptr >= end)
                    return false;
                setViewTargetString(String(viewTargetStart, ptr - viewTargetStart));
                ptr++;
            } else
                return false;
        } else if (*ptr == 'z') {
            if (!skipString(ptr, end, zoomAndPanSpec, WTF_ARRAY_LENGTH(zoomAndPanSpec)))
                return false;
            if (ptr >= end || *ptr != '(')
                return false;
            ptr++;
            if (!parseZoomAndPan(ptr, end, m_zoomAndPan))
                return false;
            if (ptr >= end || *ptr != ')')
                return false;
            ptr++;
        } else if (*ptr == 'p') {
            if (!skipString(ptr, end, preserveAspectRatioSpec, WTF_ARRAY_LENGTH(preserveAspectRatioSpec)))
                return false;
            if (ptr >= end || *ptr != '(')
                return false;
            ptr++;
            SVGPreserveAspectRatio preserveAspectRatio;
            if (!preserveAspectRatio.parse(ptr, end, false))
                return false;
            setPreserveAspectRatioBaseValue(preserveAspectRatio);
            if (ptr >= end || *ptr != ')')
                return false;
            ptr++;
        } else if (*ptr == 't') {
            if (!skipString(ptr, end, transformSpec, WTF_ARRAY_LENGTH(transformSpec)))
                return false;
            if (ptr >= end || *ptr != '(')
                return false;
            ptr++;
            SVGTransformable::parseTransformAttribute(m_transform, ptr, end, SVGTransformable::DoNotClearList);
            if (ptr >= end || *ptr != ')')
                return false;
            ptr++;
        } else
            return false;

        if (ptr < end && *ptr == ';')
            ptr++;
    }

    if (ptr >= end || *ptr != ')')
        return false;

    return true;
}

bool SVGViewSpec::parseViewSpec(const String& spec)
{
    if (spec.isEmpty() || !m_contextElement)
        return false;
    if (spec.is8Bit()) {
        const LChar* ptr = spec.characters8();
        const LChar* end = ptr + spec.length();
        return parseViewSpecInternal(ptr, end);
    }
    const UChar* ptr = spec.characters16();
    const UChar* end = ptr + spec.length();
    return parseViewSpecInternal(ptr, end);
}

}
