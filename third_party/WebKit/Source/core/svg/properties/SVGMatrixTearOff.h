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

#ifndef SVGMatrixTearOff_h
#define SVGMatrixTearOff_h

#include "core/svg/SVGTransform.h"
#include "core/svg/properties/SVGPropertyTearOff.h"

namespace WebCore {

class SVGMatrixTearOff : public SVGPropertyTearOff<SVGMatrix> {
public:
    // Used for child types (baseVal/animVal) of a SVGAnimated* property (for example: SVGAnimatedLength::baseVal()).
    // Also used for list tear offs (for example: text.x.baseVal.getItem(0)).
    static PassRefPtr<SVGMatrixTearOff> create(SVGAnimatedProperty* animatedProperty, SVGPropertyRole role, SVGMatrix& value)
    {
        ASSERT(animatedProperty);
        return adoptRef(new SVGMatrixTearOff(animatedProperty, role, value));
    }

    // Used for non-animated POD types (for example: SVGSVGElement::createSVGLength()).
    static PassRefPtr<SVGMatrixTearOff> create(const SVGMatrix& initialValue)
    {
        return adoptRef(new SVGMatrixTearOff(initialValue));
    }

    // Used for non-animated POD types that are not associated with a SVGAnimatedProperty object, nor with a XML DOM attribute
    // and that contain a parent type that's exposed to the bindings via a SVGStaticPropertyTearOff object
    // (for example: SVGTransform::matrix).
    static PassRefPtr<SVGMatrixTearOff> create(SVGPropertyTearOff<SVGTransform>* parent, SVGMatrix& value)
    {
        ASSERT(parent);
        RefPtr<SVGMatrixTearOff> result = adoptRef(new SVGMatrixTearOff(parent, value));
        parent->addChild(result->m_weakFactory.createWeakPtr());
        return result.release();
    }

    virtual void commitChange()
    {
        if (m_parent) {
            // This is a tear-off from a SVGPropertyTearOff<SVGTransform>.
            m_parent->propertyReference().updateSVGMatrix();
            m_parent->commitChange();
        } else {
            // This is either a detached tear-off or a reference tear-off from a AnimatedProperty.
            SVGPropertyTearOff<SVGMatrix>::commitChange();
        }
    }

    // SVGMatrixTearOff can be a child tear-off of a SVGTransform tear-off,
    // which means that |m_value| may be pointing inside |m_value| of the other tear-off.
    // This method is called from the parent SVGTransform tear-off when |m_parent->m_value| is updated,
    // so that |this->m_value| would point to valid location.
    virtual void setValueForMatrixIfNeeded(SVGTransform* transform)
    {
        setValue(transform->svgMatrix());
    }

    SVGPropertyTearOff<SVGTransform>* parent() { return m_parent; }

private:
    SVGMatrixTearOff(SVGAnimatedProperty* animatedProperty, SVGPropertyRole role, SVGMatrix& value)
        : SVGPropertyTearOff<SVGMatrix>(animatedProperty, role, value)
        , m_parent(0)
        , m_weakFactory(this)
    {
    }

    SVGMatrixTearOff(const SVGMatrix& initialValue)
        : SVGPropertyTearOff<SVGMatrix>(initialValue)
        , m_parent(0)
        , m_weakFactory(this)
    {
    }

    SVGMatrixTearOff(SVGPropertyTearOff<SVGTransform>* parent, SVGMatrix& value)
        : SVGPropertyTearOff<SVGMatrix>(0, UndefinedRole, value)
        , m_parent(parent)
        , m_weakFactory(this)
    {
    }

    // m_parent is kept alive from V8 wrapper.
    SVGPropertyTearOff<SVGTransform>* m_parent;

    WeakPtrFactory<SVGPropertyTearOffBase > m_weakFactory;
};

}

#endif // SVGMatrixTearOff_h
