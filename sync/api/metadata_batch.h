// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_METADATA_BATCH_H_
#define SYNC_API_METADATA_BATCH_H_

#include <map>
#include <string>

#include "sync/base/sync_export.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/entity_metadata.pb.h"

namespace syncer_v2 {

// Map of client tag to EntityMetadata proto.
typedef std::map<std::string, sync_pb::EntityMetadata> EntityMetadataMap;

// Container used to pass sync metadata from services to their processor.
class SYNC_EXPORT MetadataBatch {
 public:
  MetadataBatch();
  virtual ~MetadataBatch();

  // Allows the caller to take ownership of the entire metadata map. This is
  // done because the caller will probably swap out all the EntityMetadata
  // protos from the map for performance reasons.
  EntityMetadataMap&& TakeAllMetadata();

  // Add |metadata| for |client_tag| to the batch.
  void AddMetadata(const std::string& client_tag,
                   const sync_pb::EntityMetadata& metadata);

  // Get the DataTypeState for this batch.
  const sync_pb::DataTypeState& GetDataTypeState() const;

  // Set the DataTypeState for this batch.
  void SetDataTypeState(const sync_pb::DataTypeState& state);

 private:
  EntityMetadataMap metadata_map_;
  sync_pb::DataTypeState state_;
};

}  // namespace syncer_v2

#endif  // SYNC_API_METADATA_BATCH_H_
