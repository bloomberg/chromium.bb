/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "core/svg/properties/SVGAnimatedProperty.h"

#include "core/svg/SVGElement.h"

namespace WebCore {

SVGAnimatedProperty::SVGAnimatedProperty(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType)
    : m_contextElement(contextElement)
    , m_attributeName(attributeName)
    , m_animatedPropertyType(animatedPropertyType)
    , m_isAnimating(false)
    , m_isReadOnly(false)
{
}

SVGAnimatedProperty::~SVGAnimatedProperty()
{
    // Assure that animationEnded() was called, if animationStarted() was called before.
    ASSERT(!m_isAnimating);
}

void SVGAnimatedProperty::detachAnimatedPropertiesWrappersForElement(SVGElement* element)
{
    Cache* cache = animatedPropertyCache();
    const Cache::const_iterator end = cache->end();
    for (Cache::const_iterator it = cache->begin(); it != end; ++it) {
        if (it->key.m_element == element)
            it->value->detachWrappers();
    }
}

void SVGAnimatedProperty::detachAnimatedPropertiesForElement(SVGElement* element)
{
    // Remove wrappers from cache.
    Cache* cache = animatedPropertyCache();

    Vector<SVGAnimatedPropertyDescription> keysToRemove;

    const Cache::const_iterator end = cache->end();
    for (Cache::const_iterator it = cache->begin(); it != end; ++it) {
        if (it->key.m_element == element) {
            it->value->resetContextElement();
            keysToRemove.append(it->key);
        }
    }

    for (Vector<SVGAnimatedPropertyDescription>::const_iterator it = keysToRemove.begin(); it != keysToRemove.end(); ++it)
        cache->remove(*it);
}

void SVGAnimatedProperty::commitChange()
{
    ASSERT(m_contextElement);
    ASSERT(!m_contextElement->m_deletionHasBegun);
    m_contextElement->invalidateSVGAttributes();
    m_contextElement->svgAttributeChanged(m_attributeName);
}

SVGAnimatedProperty::Cache* SVGAnimatedProperty::animatedPropertyCache()
{
    static Cache* s_cache = new Cache;
    return s_cache;
}

} // namespace WebCore
