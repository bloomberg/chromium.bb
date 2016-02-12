// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGAnimatedHref.h"

#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGElement.h"

namespace blink {

PassRefPtrWillBeRawPtr<SVGAnimatedHref> SVGAnimatedHref::create(SVGElement* contextElement)
{
    return adoptRefWillBeNoop(new SVGAnimatedHref(contextElement));
}

DEFINE_TRACE(SVGAnimatedHref)
{
    visitor->trace(m_href);
    visitor->trace(m_xlinkHref);
    SVGAnimatedString::trace(visitor);
}

SVGAnimatedHref::SVGAnimatedHref(SVGElement* contextElement)
    : SVGAnimatedString(contextElement, SVGNames::hrefAttr, nullptr)
    , m_href(SVGAnimatedString::create(contextElement, SVGNames::hrefAttr, SVGString::create()))
    , m_xlinkHref(SVGAnimatedString::create(contextElement, XLinkNames::hrefAttr, SVGString::create()))
{
}

void SVGAnimatedHref::addToPropertyMap(SVGElement* element)
{
    element->addToPropertyMap(m_href);
    element->addToPropertyMap(m_xlinkHref);
}

bool SVGAnimatedHref::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName.matches(SVGNames::hrefAttr) || attrName.matches(XLinkNames::hrefAttr);
}

SVGPropertyBase* SVGAnimatedHref::currentValueBase()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

const SVGPropertyBase& SVGAnimatedHref::baseValueBase() const
{
    ASSERT_NOT_REACHED();
    return SVGAnimatedString::baseValueBase();
}

bool SVGAnimatedHref::isAnimating() const
{
    ASSERT_NOT_REACHED();
    return false;
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGAnimatedHref::createAnimatedValue()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void SVGAnimatedHref::setAnimatedValue(PassRefPtrWillBeRawPtr<SVGPropertyBase>)
{
    ASSERT_NOT_REACHED();
}

void SVGAnimatedHref::animationEnded()
{
    ASSERT_NOT_REACHED();
}

SVGParsingError SVGAnimatedHref::setBaseValueAsString(const String&)
{
    ASSERT_NOT_REACHED();
    return SVGParseStatus::NoError;
}

bool SVGAnimatedHref::needsSynchronizeAttribute()
{
    ASSERT_NOT_REACHED();
    return false;
}

void SVGAnimatedHref::synchronizeAttribute()
{
    ASSERT_NOT_REACHED();
}

String SVGAnimatedHref::baseVal()
{
    UseCounter::count(contextElement()->document(), UseCounter::SVGHrefBaseVal);
    return backingHref()->baseVal();
}

void SVGAnimatedHref::setBaseVal(const String& value, ExceptionState& exceptionState)
{
    UseCounter::count(contextElement()->document(), UseCounter::SVGHrefBaseVal);
    backingHref()->setBaseVal(value, exceptionState);
}

String SVGAnimatedHref::animVal()
{
    UseCounter::count(contextElement()->document(), UseCounter::SVGHrefAnimVal);
    // We should only animate (non-XLink) 'href'.
    return m_href->animVal();
}

SVGAnimatedString* SVGAnimatedHref::backingHref() const
{
    if (m_href->isSpecified())
        return m_href.get();
    if (m_xlinkHref->isSpecified())
        return m_xlinkHref.get();
    return m_href.get();
}

} // namespace blink
