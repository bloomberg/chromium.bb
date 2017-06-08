// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#if DCHECK_IS_ON()
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#endif

namespace blink {

DisplayItemClient::CacheGenerationOrInvalidationReason::ValueType
    DisplayItemClient::CacheGenerationOrInvalidationReason::next_generation_ =
        kFirstValidGeneration;

#if DCHECK_IS_ON()

HashSet<const DisplayItemClient*>* g_live_display_item_clients = nullptr;

DisplayItemClient::DisplayItemClient() {
  if (!g_live_display_item_clients)
    g_live_display_item_clients = new HashSet<const DisplayItemClient*>();
  g_live_display_item_clients->insert(this);
}

DisplayItemClient::~DisplayItemClient() {
  g_live_display_item_clients->erase(this);
}

bool DisplayItemClient::IsAlive() const {
  return g_live_display_item_clients &&
         g_live_display_item_clients->Contains(this);
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
