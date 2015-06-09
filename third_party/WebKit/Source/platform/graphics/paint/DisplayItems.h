// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItems_h
#define DisplayItems_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

#include <iterator>

class GraphicsContext;
class SkPicture;

namespace blink {

// Encapsulates logic for dealing with a simple list of display items, as
// opposed to DisplayItemList, which also manages the logic required for caching
// and updating them.
//
// This is presently implemented on top of a vector of pointers, but the
// implementation is intended to change in the future.
class PLATFORM_EXPORT DisplayItems {
    WTF_MAKE_NONCOPYABLE(DisplayItems);
public:
    // Holds a handle to an individual item, and provides restricted access to
    // it. This limits what needs to be exposed by later implementations.
    class PLATFORM_EXPORT ItemHandle {
    public:
        // Returns true if there is no item, because it was removed from the
        // list for checking of cached items.
        // TODO(jbroman): Clarify the purpose of this.
        bool isGone() const { return !m_ptr; }

        DisplayItemClient client() const { return m_ptr->client(); }
        DisplayItem::Type type() const { return m_ptr->type(); }
        DisplayItem::Id id() const { return m_ptr->id(); }

        bool isBegin() const { return m_ptr->isBegin(); }
        bool drawsContent() const { return m_ptr->drawsContent(); }
        bool skippedCache() const { return m_ptr->skippedCache(); }

        void appendToWebDisplayItemList(WebDisplayItemList* list) const
        {
            m_ptr->appendToWebDisplayItemList(list);
        }

        const SkPicture* picture() const
        {
            ASSERT_WITH_SECURITY_IMPLICATION(DisplayItem::isDrawingType(type()));
            return static_cast<DrawingDisplayItem*>(m_ptr)->picture();
        }

#if ENABLE(ASSERT)
        DrawingDisplayItem::UnderInvalidationCheckingMode underInvalidationCheckingMode() const
        {
            ASSERT_WITH_SECURITY_IMPLICATION(DisplayItem::isDrawingType(type()));
            return static_cast<DrawingDisplayItem*>(m_ptr)->underInvalidationCheckingMode();
        }
#endif

#ifndef NDEBUG
        String asDebugString() const { return m_ptr->asDebugString(); }
        void dumpPropertiesAsDebugString(StringBuilder& builder) const { m_ptr->dumpPropertiesAsDebugString(builder); }
#endif

        void replay(GraphicsContext& context) const { m_ptr->replay(context); }

    private:
        ItemHandle() : m_ptr(nullptr) { }
        explicit ItemHandle(DisplayItem* ptr) : m_ptr(ptr) { }

        DisplayItem* m_ptr;
        friend class DisplayItems;
    };

    // Input iterators over the display items.
    class Iterator : public std::iterator<std::input_iterator_tag, const ItemHandle> {
    public:
        const ItemHandle& operator*() const { m_handle = ItemHandle(m_iterator->get()); return m_handle; }
        const ItemHandle* operator->() const { return &operator*(); }
        Iterator& operator++() { ++m_iterator; return *this; }
        Iterator operator++(int) { Iterator tmp(*this); operator++(); return tmp; }
        bool operator==(const Iterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const Iterator& other) const { return !(*this == other); }
    private:
        using InternalIterator = Vector<OwnPtr<DisplayItem>>::iterator;
        Iterator(const InternalIterator& it) : m_iterator(it) { }
        InternalIterator m_iterator;
        mutable ItemHandle m_handle;
        friend class DisplayItems;
        friend class ConstIterator;
    };
    class ConstIterator : public std::iterator<std::input_iterator_tag, const ItemHandle> {
    public:
        ConstIterator(const Iterator& it) : m_iterator(it.m_iterator) { }
        const ItemHandle& operator*() const { m_handle = ItemHandle(m_iterator->get()); return m_handle; }
        const ItemHandle* operator->() const { return &operator*(); }
        ConstIterator& operator++() { ++m_iterator; return *this; }
        ConstIterator operator++(int) { ConstIterator tmp(*this); operator++(); return tmp; }
        bool operator==(const ConstIterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const ConstIterator& other) const { return !(*this == other); }
    private:
        using InternalIterator = Vector<OwnPtr<DisplayItem>>::const_iterator;
        ConstIterator(const InternalIterator& it) : m_iterator(it) { }
        InternalIterator m_iterator;
        mutable ItemHandle m_handle;
        friend class DisplayItems;
    };

    DisplayItems();
    ~DisplayItems();

    bool isEmpty() const { return m_items.isEmpty(); }
    size_t size() const { return m_items.size(); }

    // Random access may not remain O(1) in the future.
    ItemHandle operator[](size_t index) const { return ItemHandle(m_items[index].get()); }
    Iterator iteratorAt(size_t index) { return Iterator(m_items.begin() + index); }
    ConstIterator iteratorAt(size_t index) const { return ConstIterator(m_items.begin() + index); }
    size_t indexForIterator(const Iterator& it) const { return it.m_iterator - m_items.begin(); }
    size_t indexForIterator(const ConstIterator& it) const { return it.m_iterator - m_items.begin(); }

    // Input iteration, however, should remain cheap.
    Iterator begin() { return Iterator(m_items.begin()); }
    Iterator end() { return Iterator(m_items.end()); }
    ConstIterator begin() const { return ConstIterator(m_items.begin()); }
    ConstIterator end() const { return ConstIterator(m_items.end()); }

    // Access to the end of the list should also be fast.
    ItemHandle last() const { return ItemHandle(m_items.last().get()); }

    // TODO(jbroman): Replace this with something that doesn't first require
    // heap allocation.
    void append(PassOwnPtr<DisplayItem>);

    // Appends by moving from another list. The item in the list it's being
    // removed from will become "gone" in this process, but can still be safely
    // destroyed.
    void appendByMoving(const Iterator&);

    void removeLast();
    void clear();

    // TODO(jbroman): Is swap the right primitive?
    void swap(DisplayItems&);

    // TODO(jbroman): This abstraction is odd and it would be nice to explain it
    // more clearly.
    void setGone(const Iterator&);

private:
    Vector<OwnPtr<DisplayItem>> m_items;
};

} // namespace blink

#endif // DisplayItems_h
