// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ENTITY_DATA_H_
#define SYNC_API_ENTITY_DATA_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/proto_value_ptr.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {

struct EntityData;

struct SYNC_EXPORT EntityDataTraits {
  static void SwapValue(EntityData* dest, EntityData* src);
  static bool HasValue(const EntityData& value);
  static const EntityData& DefaultValue();
};

typedef syncer::ProtoValuePtr<EntityData, EntityDataTraits> EntityDataPtr;
typedef std::vector<EntityDataPtr> EntityDataList;

// A light-weight container for sync entity data which represents either
// local data created on the ModelTypeService side or remote data created
// on ModelTypeWorker.
// EntityData is supposed to be wrapped and passed by reference.
struct SYNC_EXPORT EntityData {
 public:
  EntityData();
  ~EntityData();

  // Typically this is a server assigned sync ID, although for a local change
  // that represents a new entity this field might be either empty or contain
  // a temporary client sync ID.
  std::string id;

  // A hash based on the client tag and model type.
  // Used for various map lookups. Should always be available.
  // Sent to the server as SyncEntity::client_defined_unique_tag.
  std::string client_tag_hash;

  // Entity name, used mostly for Debug purposes.
  std::string non_unique_name;

  // Model type specific sync data.
  sync_pb::EntitySpecifics specifics;

  // Entity creation and modification timestamps.
  base::Time creation_time;
  base::Time modification_time;

  // Sync ID of the parent entity. This is supposed to be set only for
  // hierarchical datatypes (e.g. Bookmarks).
  std::string parent_id;

  // Unique position of an entity among its siblings. This is supposed to be
  // set only for datatypes that support positioning (e.g. Bookmarks).
  sync_pb::UniquePosition unique_position;

  // True if EntityData represents deleted entity; otherwise false.
  // Note that EntityData would be considered to represent a deletion if its
  // specifics hasn't been set.
  bool is_deleted() const { return specifics.ByteSize() == 0; }

  // Transfers this struct's data to EntityDataPtr.
  // The return value must be assigned into another EntityDataPtr.
  EntityDataPtr PassToPtr() WARN_UNUSED_RESULT;

 private:
  friend struct EntityDataTraits;
  // Used to transfer the data without copying.
  void Swap(EntityData* other);

  DISALLOW_COPY_AND_ASSIGN(EntityData);
};

}  // namespace syncer_v2

#endif  // SYNC_API_ENTITY_DATA_H_
