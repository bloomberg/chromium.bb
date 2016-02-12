// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/entity_data.h"

#include <algorithm>

#include "base/logging.h"

namespace syncer_v2 {

EntityData::EntityData() {}
EntityData::~EntityData() {}

void EntityData::Swap(EntityData* other) {
  id.swap(other->id);
  client_tag_hash.swap(other->client_tag_hash);
  non_unique_name.swap(other->non_unique_name);

  specifics.Swap(&other->specifics);

  std::swap(creation_time, other->creation_time);
  std::swap(modification_time, other->modification_time);

  parent_id.swap(other->parent_id);
  unique_position.Swap(&other->unique_position);
}

EntityDataPtr EntityData::PassToPtr() {
  EntityDataPtr target;
  target.swap_value(this);
  return target;
}

void EntityDataTraits::SwapValue(EntityData* dest, EntityData* src) {
  dest->Swap(src);
}

bool EntityDataTraits::HasValue(const EntityData& value) {
  return !value.client_tag_hash.empty();
}

const EntityData& EntityDataTraits::DefaultValue() {
  CR_DEFINE_STATIC_LOCAL(EntityData, default_instance, ());
  return default_instance;
}

}  // namespace syncer_v2
