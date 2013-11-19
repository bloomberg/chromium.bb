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

#ifndef SVGStaticListPropertyTearOff_h
#define SVGStaticListPropertyTearOff_h

#include "core/svg/properties/SVGListProperty.h"

namespace WebCore {

class ExceptionState;

template<typename PropertyType>
class SVGStaticListPropertyTearOff : public SVGListProperty<PropertyType> {
public:
    typedef SVGListProperty<PropertyType> Base;

    typedef typename SVGPropertyTraits<PropertyType>::ListItemType ListItemType;
    typedef SVGPropertyTearOff<ListItemType> ListItemTearOff;

    using Base::m_role;
    using Base::m_values;

    static PassRefPtr<SVGStaticListPropertyTearOff<PropertyType> > create(SVGElement* contextElement, PropertyType& values)
    {
        ASSERT(contextElement);
        return adoptRef(new SVGStaticListPropertyTearOff<PropertyType>(contextElement, values));
    }

    SVGElement* contextElement() const { return m_contextElement; }

    // SVGList API
    void clear(ExceptionState& exceptionState)
    {
        Base::clearValues(exceptionState);
    }

    ListItemType initialize(const ListItemType& newItem, ExceptionState& exceptionState)
    {
        return Base::initializeValues(newItem, exceptionState);
    }

    ListItemType getItem(unsigned index, ExceptionState& exceptionState)
    {
        return Base::getItemValues(index, exceptionState);
    }

    ListItemType insertItemBefore(const ListItemType& newItem, unsigned index, ExceptionState& exceptionState)
    {
        return Base::insertItemBeforeValues(newItem, index, exceptionState);
    }

    ListItemType replaceItem(const ListItemType& newItem, unsigned index, ExceptionState& exceptionState)
    {
        return Base::replaceItemValues(newItem, index, exceptionState);
    }

    ListItemType removeItem(unsigned index, ExceptionState& exceptionState)
    {
        return Base::removeItemValues(index, exceptionState);
    }

    ListItemType appendItem(const ListItemType& newItem, ExceptionState& exceptionState)
    {
        return Base::appendItemValues(newItem, exceptionState);
    }

private:
    SVGStaticListPropertyTearOff(SVGElement* contextElement, PropertyType& values)
        : SVGListProperty<PropertyType>(UndefinedRole, values, 0)
        , m_contextElement(contextElement)
    {
        m_contextElement->setContextElement();
    }

    virtual bool isReadOnly() const
    {
        return m_role == AnimValRole;
    }

    virtual void commitChange()
    {
        ASSERT(m_values);
        ASSERT(m_contextElement);
        m_values->commitChange(m_contextElement);
    }

    virtual bool processIncomingListItemValue(const ListItemType&, unsigned*)
    {
        // no-op for static lists
        return true;
    }

    virtual bool processIncomingListItemWrapper(RefPtr<ListItemTearOff>&, unsigned*)
    {
        ASSERT_NOT_REACHED();
        return true;
    }

private:
    SVGElement* m_contextElement;
};

}

#endif // SVGStaticListPropertyTearOff_h
