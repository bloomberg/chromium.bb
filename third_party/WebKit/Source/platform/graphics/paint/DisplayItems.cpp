// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItems.h"

namespace blink {

DisplayItems::DisplayItems()
{
}

DisplayItems::~DisplayItems()
{
}

void DisplayItems::append(PassOwnPtr<DisplayItem> displayItem)
{
    m_items.append(displayItem);
}

void DisplayItems::appendByMoving(const Iterator& it)
{
    // Release the underlying OwnPtr to move the display item ownership.
    ASSERT(!it->isGone());
    append(it.m_iterator->release());
}

void DisplayItems::removeLast()
{
    m_items.removeLast();
}

void DisplayItems::clear()
{
    m_items.clear();
}

void DisplayItems::swap(DisplayItems& other)
{
    m_items.swap(other.m_items);
}

void DisplayItems::setGone(const Iterator& it)
{
    it.m_iterator->clear();
}

} // namespace blink
