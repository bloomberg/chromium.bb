// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SIMPLE_METADATA_CHANGE_LIST_H_
#define SYNC_INTERNAL_API_PUBLIC_SIMPLE_METADATA_CHANGE_LIST_H_

#include <map>
#include <string>

#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_store.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/entity_metadata.pb.h"

namespace syncer_v2 {

// A MetadataChangeList implementation that is meant to be used in combination
// with a ModelTypeStore. It accumulates changes in member fields, and then when
// requested transfers them to the store/write batch.
class SYNC_EXPORT SimpleMetadataChangeList : public MetadataChangeList {
 public:
  enum ChangeType { UPDATE, CLEAR };

  struct MetadataChange {
    ChangeType type;
    sync_pb::EntityMetadata metadata;
  };

  struct DataTypeStateChange {
    ChangeType type;
    sync_pb::DataTypeState state;
  };

  typedef std::map<std::string, MetadataChange> MetadataChanges;

  SimpleMetadataChangeList();
  ~SimpleMetadataChangeList() override;

  void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) override;
  void ClearDataTypeState() override;
  void UpdateMetadata(const std::string& client_tag,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& client_tag) override;

  const MetadataChanges& GetMetadataChanges() const;
  bool HasDataTypeStateChange() const;
  const DataTypeStateChange& GetDataTypeStateChange() const;

  // Moves all currently accumulated changes into the write batch, clear out
  // local copies. Calling this multiple times will work, but should not be
  // necessary.
  void TransferChanges(ModelTypeStore* store,
                       ModelTypeStore::WriteBatch* write_batch);

 private:
  MetadataChanges metadata_changes_;
  std::unique_ptr<DataTypeStateChange> state_change_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_SIMPLE_METADATA_CHANGE_LIST_H_
