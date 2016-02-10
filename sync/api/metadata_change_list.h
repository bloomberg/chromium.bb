// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_METADATA_CHANGE_LIST_H_
#define SYNC_API_METADATA_CHANGE_LIST_H_

#include <string>

#include "sync/base/sync_export.h"

namespace sync_pb {
class DataTypeState;
class EntityMetadata;
}  // namespace sync_pb

namespace syncer_v2 {

// Interface used by the processor and service to communicate about metadata.
// The purpose of the interface is to record changes to data type global and
// per entity metadata for the purpose of propagating changes to the datatype
// specific storage implementation.
// The implementation of the interface is supposed to keep the record of all
// updated / deleted metadata records and provide a mechanism to enumerate
// them. If there are multiple UpdateMetadata / ClearMetadata calls made for the
// same metadata record the last one is supposed to win.
class SYNC_EXPORT MetadataChangeList {
 public:
  MetadataChangeList() {}
  virtual ~MetadataChangeList() {}

  // Requests DataTypeState to be updated in the storage.
  virtual void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) = 0;

  // Requests DataTypeState to be cleared from the storage.
  virtual void ClearDataTypeState() = 0;

  // Requests metadata entry to be updated in the storage.
  // Please note that the update might contain a deleted entry if
  // metadata.is_deleted() is true (as opposed to clearing the entry from the
  // storage completely by calling the Clear method).
  virtual void UpdateMetadata(const std::string& client_tag,
                              const sync_pb::EntityMetadata& metadata) = 0;

  // Requests metadata entry to be cleared from the storage.
  virtual void ClearMetadata(const std::string& client_tag) = 0;
};

}  // namespace syncer_v2

#endif  // SYNC_API_METADATA_CHANGE_LIST_H_
