// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/entity_data.h"

#include "sync/protocol/entity_metadata.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/time.h"

namespace syncer_v2 {

EntityData::EntityData(const std::string& client_id,
                       const std::string& non_unique_name,
                       const sync_pb::EntitySpecifics* specifics,
                       bool is_deleted)
    : client_id_(client_id),
      non_unique_name_(non_unique_name),
      is_deleted_(is_deleted) {
  if (!is_deleted) {
    DCHECK(specifics);
    specifics_.set_value(*specifics);
  }
}

EntityData::~EntityData() {}

// Static.
EntityData EntityData::CreateData(const std::string& client_id,
                                  const std::string& non_unique_name,
                                  const sync_pb::EntitySpecifics& specifics) {
  return EntityData(client_id, non_unique_name, &specifics, false);
}

// Static.
EntityData EntityData::CreateDelete(const std::string& sync_tag,
                                    const std::string& non_unique_name) {
  return EntityData(sync_tag, non_unique_name, nullptr, true);
}

const sync_pb::EntitySpecifics& EntityData::specifics() const {
  DCHECK(!is_deleted());
  return specifics_.value();
}

bool EntityData::HasMetadata() const {
  return metadata_.client_id() == client_id_;
}

void EntityData::SetMetadata(const sync_pb::EntityMetadata& metadata) {
  DCHECK(!HasMetadata());
  DCHECK_EQ(client_id_, metadata.client_id());
  modification_time_ = syncer::ProtoTimeToTime(metadata.modification_time());
  metadata_ = metadata;
}

}  // namespace syncer_v2
