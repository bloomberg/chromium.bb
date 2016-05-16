// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#if ENABLE(ASSERT)

#include "wtf/HashSet.h"

namespace blink {

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

} // namespace blink

#endif // ENABLE(ASSERT)
