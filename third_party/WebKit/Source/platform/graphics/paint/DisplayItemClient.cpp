// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#endif

namespace blink {

DisplayItemClient::CacheGenerationOrInvalidationReason::ValueType
    DisplayItemClient::CacheGenerationOrInvalidationReason::next_generation_ =
        kFirstValidGeneration;

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS

HashSet<const DisplayItemClient*>* g_live_display_item_clients = nullptr;
HashMap<const void*, HashMap<const DisplayItemClient*, String>>*
    g_display_item_clients_should_keep_alive = nullptr;

DisplayItemClient::DisplayItemClient() {
  if (g_display_item_clients_should_keep_alive) {
    for (const auto& item : *g_display_item_clients_should_keep_alive)
      CHECK(!item.value.Contains(this));
  }
  if (!g_live_display_item_clients)
    g_live_display_item_clients = new HashSet<const DisplayItemClient*>();
  g_live_display_item_clients->insert(this);
}

DisplayItemClient::~DisplayItemClient() {
  if (g_display_item_clients_should_keep_alive) {
    for (const auto& item : *g_display_item_clients_should_keep_alive) {
      CHECK(!item.value.Contains(this))
          << "Short-lived DisplayItemClient: " << item.value.at(this)
          << ". See crbug.com/609218.";
    }
  }
  g_live_display_item_clients->erase(this);
  // In case this object is a subsequence owner.
  EndShouldKeepAliveAllClients(this);
}

bool DisplayItemClient::IsAlive() const {
  return g_live_display_item_clients &&
         g_live_display_item_clients->Contains(this);
}

void DisplayItemClient::BeginShouldKeepAlive(const void* owner) const {
  CHECK(IsAlive());
  if (!g_display_item_clients_should_keep_alive)
    g_display_item_clients_should_keep_alive =
        new HashMap<const void*, HashMap<const DisplayItemClient*, String>>();
  auto add_result =
      g_display_item_clients_should_keep_alive
          ->insert(owner, HashMap<const DisplayItemClient*, String>())
          .stored_value->value.insert(this, "");
  if (add_result.is_new_entry)
    add_result.stored_value->value = DebugName();
}

void DisplayItemClient::EndShouldKeepAlive() const {
  if (g_display_item_clients_should_keep_alive) {
    for (auto& item : *g_display_item_clients_should_keep_alive)
      item.value.erase(this);
  }
}

void DisplayItemClient::EndShouldKeepAliveAllClients(const void* owner) {
  if (g_display_item_clients_should_keep_alive)
    g_display_item_clients_should_keep_alive->erase(owner);
}

void DisplayItemClient::EndShouldKeepAliveAllClients() {
  delete g_display_item_clients_should_keep_alive;
  g_display_item_clients_should_keep_alive = nullptr;
}

#endif  // CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS

}  // namespace blink
