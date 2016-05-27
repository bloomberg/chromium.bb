// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#endif

namespace blink {

DisplayItemCacheGeneration::Generation DisplayItemCacheGeneration::s_nextGeneration = 1;

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS

HashSet<const DisplayItemClient*>* liveDisplayItemClients = nullptr;
HashMap<const DisplayItemClient*, String>* displayItemClientsShouldKeepAlive = nullptr;

DisplayItemClient::DisplayItemClient()
{
    CHECK(!displayItemClientsShouldKeepAlive || !displayItemClientsShouldKeepAlive->contains(this));
    if (!liveDisplayItemClients)
        liveDisplayItemClients = new HashSet<const DisplayItemClient*>();
    liveDisplayItemClients->add(this);
}

DisplayItemClient::~DisplayItemClient()
{
    CHECK(!displayItemClientsShouldKeepAlive || !displayItemClientsShouldKeepAlive->contains(this))
        << "Short-lived DisplayItemClient: " << displayItemClientsShouldKeepAlive->get(this)
        << ". See crbug.com/570030.";
    liveDisplayItemClients->remove(this);
}

bool DisplayItemClient::isAlive() const
{
    return liveDisplayItemClients && liveDisplayItemClients->contains(this);
}

void DisplayItemClient::beginShouldKeepAlive() const
{
    CHECK(isAlive());
    if (!displayItemClientsShouldKeepAlive)
        displayItemClientsShouldKeepAlive = new HashMap<const DisplayItemClient*, String>();
#ifdef NDEBUG
    displayItemClientsShouldKeepAlive->add(this, "");
#else
    auto addResult = displayItemClientsShouldKeepAlive->add(this, "");
    if (addResult.isNewEntry)
        addResult.storedValue->value = debugName();
#endif
}

void DisplayItemClient::endShouldKeepAliveAllClients()
{
    delete displayItemClientsShouldKeepAlive;
    displayItemClientsShouldKeepAlive = nullptr;
}

#endif // CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS

} // namespace blink
