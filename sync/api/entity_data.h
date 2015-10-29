// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ENTITY_DATA_H_
#define SYNC_API_ENTITY_DATA_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/proto_value_ptr.h"
#include "sync/protocol/entity_metadata.pb.h"

namespace sync_pb {
class EntityMetadata;
class EntitySpecifics;
}  // namespace sync_pb

namespace syncer_v2 {

// A light-weight container for sync entity data.
//
// EntityData objects either have specifics data or represent a deleted
// entity that does not have specifics. This component is immutable.
//
// EntityData objects can also be modified by adding EntityMetadata to them,
// which is stored in its serialized form for persistence to storage.
class SYNC_EXPORT EntityData {
 public:
  ~EntityData();

  // Default copy and assign welcome.

  static EntityData CreateData(const std::string& client_id,
                               const std::string& non_unique_name,
                               const sync_pb::EntitySpecifics& specifics);

  static EntityData CreateDelete(const std::string& client_id,
                                 const std::string& non_unique_name);

  const std::string& client_id() const { return client_id_; }
  const std::string& non_unique_name() const { return non_unique_name_; }
  bool is_deleted() const { return is_deleted_; }
  const sync_pb::EntitySpecifics& specifics() const;
  bool HasMetadata() const;
  void SetMetadata(const sync_pb::EntityMetadata& metadata);
  // Maybe this should be private with SMTP and ModelTypeWorker as friends?
  const sync_pb::EntityMetadata& GetMetadata() const { return metadata_; }

 private:
  EntityData(const std::string& client_id,
             const std::string& non_unique_name,
             const sync_pb::EntitySpecifics* specifics,
             bool is_deleted);

  // Fields that are always present and don't change.
  const std::string client_id_;
  const std::string non_unique_name_;
  syncer::syncable::EntitySpecificsPtr specifics_;
  // TODO(maxbogue): This might be replaced by a ChangeType enum.
  const bool is_deleted_;

  // Present sometimes.
  base::Time modification_time_;
  sync_pb::EntityMetadata metadata_;

  // Only set for bookmarks; should really be in bookmark specifics.
  // std::string parent_tag_hash_;
  // sync_pb::UniquePosition unique_position_;
};

typedef std::vector<EntityData> EntityDataList;

}  // namespace syncer_v2

#endif  // SYNC_API_ENTITY_DATA_H_
