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

#ifndef SVGListProperty_h
#define SVGListProperty_h

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/properties/SVGPropertyTearOff.h"
#include "core/svg/properties/SVGPropertyTraits.h"

namespace WebCore {

enum ListModification {
    ListModificationUnknown = 0,
    ListModificationInsert = 1,
    ListModificationReplace = 2,
    ListModificationRemove = 3,
    ListModificationAppend = 4
};

template<typename PropertyType>
class SVGAnimatedListPropertyTearOff;

template<typename PropertyType>
class SVGListProperty : public SVGProperty {
public:
    typedef SVGListProperty<PropertyType> Self;

    typedef typename SVGPropertyTraits<PropertyType>::ListItemType ListItemType;
    typedef SVGPropertyTearOff<ListItemType> ListItemTearOff;
    typedef PassRefPtr<ListItemTearOff> PassListItemTearOff;
    typedef SVGAnimatedListPropertyTearOff<PropertyType> AnimatedListPropertyTearOff;
    typedef typename SVGAnimatedListPropertyTearOff<PropertyType>::ListWrapperCache ListWrapperCache;

    bool canAlterList(ExceptionState& es) const
    {
        if (m_role == AnimValRole) {
            es.throwUninformativeAndGenericDOMException(NoModificationAllowedError);
            return false;
        }

        return true;
    }

    static void detachListWrappersAndResize(ListWrapperCache* wrappers, unsigned newListSize = 0)
    {
        // See SVGPropertyTearOff::detachWrapper() for an explanation about what's happening here.
        ASSERT(wrappers);
        unsigned size = wrappers->size();
        for (unsigned i = 0; i < size; ++i) {
            if (ListItemTearOff* item = wrappers->at(i).get())
                item->detachWrapper();
        }

        // Reinitialize the wrapper cache to be equal to the new values size, after the XML DOM changed the list.
        if (newListSize)
            wrappers->fill(0, newListSize);
        else
            wrappers->clear();
    }

    void detachListWrappers(unsigned newListSize)
    {
        detachListWrappersAndResize(m_wrappers, newListSize);
    }

    void setValuesAndWrappers(PropertyType* values, ListWrapperCache* wrappers, bool shouldOwnValues)
    {
        // This is only used for animVal support, to switch the underlying values & wrappers
        // to the current animated values, once animation for a list starts.
        ASSERT(m_values);
        ASSERT(m_wrappers);
        ASSERT(m_role == AnimValRole);
        if (m_ownsValues)
            delete m_values;
        m_values = values;
        m_ownsValues = shouldOwnValues;
        m_wrappers = wrappers;
        ASSERT(m_values->size() == m_wrappers->size());
    }

    // SVGList::clear()
    void clearValues(ExceptionState& es)
    {
        if (!canAlterList(es))
            return;

        m_values->clear();
        commitChange();
    }

    void clearValuesAndWrappers(ExceptionState& es)
    {
        if (!canAlterList(es))
            return;

        detachListWrappers(0);
        m_values->clear();
        commitChange();
    }

    // SVGList::numberOfItems()
    unsigned numberOfItems() const
    {
        return m_values->size();
    }

    // SVGList::initialize()
    ListItemType initializeValues(const ListItemType& newItem, ExceptionState& es)
    {
        if (!canAlterList(es))
            return ListItemType();

        // Spec: If the inserted item is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemValue(newItem, 0);

        // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
        m_values->clear();
        m_values->append(newItem);

        commitChange();
        return newItem;
    }

    PassListItemTearOff initializeValuesAndWrappers(PassListItemTearOff passNewItem, ExceptionState& es)
    {
        ASSERT(m_wrappers);
        if (!canAlterList(es))
            return 0;

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            es.throwTypeError();
            return 0;
        }

        RefPtr<ListItemTearOff> newItem = passNewItem;
        ASSERT(m_values->size() == m_wrappers->size());

        // Spec: If the inserted item is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemWrapper(newItem, 0);

        // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
        detachListWrappers(0);
        m_values->clear();

        m_values->append(newItem->propertyReference());
        m_wrappers->append(newItem);

        commitChange();
        return newItem.release();
    }

    // SVGList::getItem()
    bool canGetItem(unsigned index, ExceptionState& es)
    {
        if (index >= m_values->size()) {
            es.throwUninformativeAndGenericDOMException(IndexSizeError);
            return false;
        }

        return true;
    }

    ListItemType getItemValues(unsigned index, ExceptionState& es)
    {
        if (!canGetItem(index, es))
            return ListItemType();

        // Spec: Returns the specified item from the list. The returned item is the item itself and not a copy.
        return m_values->at(index);
    }

    PassListItemTearOff getItemValuesAndWrappers(AnimatedListPropertyTearOff* animatedList, unsigned index, ExceptionState& es)
    {
        ASSERT(m_wrappers);
        if (!canGetItem(index, es))
            return 0;

        // Spec: Returns the specified item from the list. The returned item is the item itself and not a copy.
        // Any changes made to the item are immediately reflected in the list.
        ASSERT(m_values->size() == m_wrappers->size());
        RefPtr<ListItemTearOff> wrapper = m_wrappers->at(index);
        if (!wrapper) {
            // Create new wrapper, which is allowed to directly modify the item in the list, w/o copying and cache the wrapper in our map.
            // It is also associated with our animated property, so it can notify the SVG Element which holds the SVGAnimated*List
            // that it has been modified (and thus can call svgAttributeChanged(associatedAttributeName)).
            wrapper = ListItemTearOff::create(animatedList, UndefinedRole, m_values->at(index));
            m_wrappers->at(index) = wrapper;
        }

        return wrapper.release();
    }

    // SVGList::insertItemBefore()
    ListItemType insertItemBeforeValues(const ListItemType& newItem, unsigned index, ExceptionState& es)
    {
        if (!canAlterList(es))
            return ListItemType();

        // Spec: If the index is greater than or equal to numberOfItems, then the new item is appended to the end of the list.
        if (index > m_values->size())
            index = m_values->size();

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        if (!processIncomingListItemValue(newItem, &index)) {
            // Inserting the item before itself is a no-op.
            return newItem;
        }

        // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
        // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
        m_values->insert(index, newItem);

        commitChange();
        return newItem;
    }

    PassListItemTearOff insertItemBeforeValuesAndWrappers(PassListItemTearOff passNewItem, unsigned index, ExceptionState& es)
    {
        ASSERT(m_wrappers);
        if (!canAlterList(es))
            return 0;

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            es.throwTypeError();
            return 0;
        }

        // Spec: If the index is greater than or equal to numberOfItems, then the new item is appended to the end of the list.
        if (index > m_values->size())
             index = m_values->size();

        RefPtr<ListItemTearOff> newItem = passNewItem;
        ASSERT(m_values->size() == m_wrappers->size());

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        if (!processIncomingListItemWrapper(newItem, &index))
            return newItem.release();

        // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
        // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
        m_values->insert(index, newItem->propertyReference());

        // Store new wrapper at position 'index', change its underlying value, so mutations of newItem, directly affect the item in the list.
        m_wrappers->insert(index, newItem);

        commitChange();
        return newItem.release();
    }

    // SVGList::replaceItem()
    bool canReplaceItem(unsigned index, ExceptionState& es)
    {
        if (!canAlterList(es))
            return false;

        if (index >= m_values->size()) {
            es.throwUninformativeAndGenericDOMException(IndexSizeError);
            return false;
        }

        return true;
    }

    ListItemType replaceItemValues(const ListItemType& newItem, unsigned index, ExceptionState& es)
    {
        if (!canReplaceItem(index, es))
            return ListItemType();

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        // Spec: If the item is already in this list, note that the index of the item to replace is before the removal of the item.
        if (!processIncomingListItemValue(newItem, &index)) {
            // Replacing the item with itself is a no-op.
            return newItem;
        }

        if (m_values->isEmpty()) {
            // 'newItem' already lived in our list, we removed it, and now we're empty, which means there's nothing to replace.
            es.throwUninformativeAndGenericDOMException(IndexSizeError);
            return ListItemType();
        }

        // Update the value at the desired position 'index'.
        m_values->at(index) = newItem;

        commitChange();
        return newItem;
    }

    PassListItemTearOff replaceItemValuesAndWrappers(PassListItemTearOff passNewItem, unsigned index, ExceptionState& es)
    {
        ASSERT(m_wrappers);
        if (!canReplaceItem(index, es))
            return 0;

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            es.throwTypeError();
            return 0;
        }

        ASSERT(m_values->size() == m_wrappers->size());
        RefPtr<ListItemTearOff> newItem = passNewItem;

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        // Spec: If the item is already in this list, note that the index of the item to replace is before the removal of the item.
        if (!processIncomingListItemWrapper(newItem, &index))
            return newItem.release();

        if (m_values->isEmpty()) {
            ASSERT(m_wrappers->isEmpty());
            // 'passNewItem' already lived in our list, we removed it, and now we're empty, which means there's nothing to replace.
            es.throwUninformativeAndGenericDOMException(IndexSizeError);
            return 0;
        }

        // Detach the existing wrapper.
        RefPtr<ListItemTearOff> oldItem = m_wrappers->at(index);
        if (oldItem)
            oldItem->detachWrapper();

        // Update the value and the wrapper at the desired position 'index'.
        m_values->at(index) = newItem->propertyReference();
        m_wrappers->at(index) = newItem;

        commitChange();
        return newItem.release();
    }

    // SVGList::removeItem()
    bool canRemoveItem(unsigned index, ExceptionState& es)
    {
        if (!canAlterList(es))
            return false;

        if (index >= m_values->size()) {
            es.throwUninformativeAndGenericDOMException(IndexSizeError);
            return false;
        }

        return true;
    }

    ListItemType removeItemValues(unsigned index, ExceptionState& es)
    {
        if (!canRemoveItem(index, es))
            return ListItemType();

        ListItemType oldItem = m_values->at(index);
        m_values->remove(index);

        commitChange();
        return oldItem;
    }

    PassListItemTearOff removeItemValuesAndWrappers(AnimatedListPropertyTearOff* animatedList, unsigned index, ExceptionState& es)
    {
        ASSERT(m_wrappers);
        if (!canRemoveItem(index, es))
            return 0;

        ASSERT(m_values->size() == m_wrappers->size());

        // Detach the existing wrapper.
        RefPtr<ListItemTearOff> oldItem = m_wrappers->at(index);
        if (!oldItem)
            oldItem = ListItemTearOff::create(animatedList, UndefinedRole, m_values->at(index));

        oldItem->detachWrapper();
        m_wrappers->remove(index);
        m_values->remove(index);

        commitChange();
        return oldItem.release();
    }

    // SVGList::appendItem()
    ListItemType appendItemValues(const ListItemType& newItem, ExceptionState& es)
    {
        if (!canAlterList(es))
            return ListItemType();

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemValue(newItem, 0);

        // Append the value at the end of the list.
        m_values->append(newItem);

        commitChange(ListModificationAppend);
        return newItem;
    }

    PassListItemTearOff appendItemValuesAndWrappers(PassListItemTearOff passNewItem, ExceptionState& es)
    {
        ASSERT(m_wrappers);
        if (!canAlterList(es))
            return 0;

        // Not specified, but FF/Opera do it this way, and it's just sane.
        if (!passNewItem) {
            es.throwTypeError();
            return 0;
        }

        RefPtr<ListItemTearOff> newItem = passNewItem;
        ASSERT(m_values->size() == m_wrappers->size());

        // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
        processIncomingListItemWrapper(newItem, 0);

        // Append the value and wrapper at the end of the list.
        m_values->append(newItem->propertyReference());
        m_wrappers->append(newItem);

        commitChange(ListModificationAppend);
        return newItem.release();
    }

    PropertyType& values()
    {
        ASSERT(m_values);
        return *m_values;
    }

    ListWrapperCache& wrappers() const
    {
        ASSERT(m_wrappers);
        return *m_wrappers;
    }

protected:
    SVGListProperty(SVGPropertyRole role, PropertyType& values, ListWrapperCache* wrappers)
        : m_role(role)
        , m_ownsValues(false)
        , m_values(&values)
        , m_wrappers(wrappers)
    {
    }

    virtual ~SVGListProperty()
    {
        if (m_ownsValues)
            delete m_values;
    }

    virtual void commitChange() = 0;
    virtual void commitChange(ListModification)
    {
        commitChange();
    }

    virtual bool processIncomingListItemValue(const ListItemType& newItem, unsigned* indexToModify) = 0;
    virtual bool processIncomingListItemWrapper(RefPtr<ListItemTearOff>& newItem, unsigned* indexToModify) = 0;

    SVGPropertyRole m_role;
    bool m_ownsValues;
    PropertyType* m_values;
    ListWrapperCache* m_wrappers;
};

}

#endif // SVGListProperty_h
