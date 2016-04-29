// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#if ENABLE(ASSERT)
#include "wtf/HashSet.h"
#endif

namespace blink {

DisplayItemCacheGeneration::Generation DisplayItemCacheGeneration::s_nextGeneration = 1;

#if ENABLE(ASSERT)

HashSet<const DisplayItemClient*>* liveDisplayItemClients = nullptr;

DisplayItemClient::DisplayItemClient()
{
    if (!liveDisplayItemClients)
        liveDisplayItemClients = new HashSet<const DisplayItemClient*>();
    liveDisplayItemClients->add(this);
}

DisplayItemClient::~DisplayItemClient()
{
    liveDisplayItemClients->remove(this);
}

bool DisplayItemClient::isAlive(const DisplayItemClient& client)
{
    return liveDisplayItemClients && liveDisplayItemClients->contains(&client);
}

#endif // ENABLE(ASSERT)

} // namespace blink
