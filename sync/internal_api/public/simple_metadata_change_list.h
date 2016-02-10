// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SIMPLE_METADATA_CHANGE_LIST_H_
#define SYNC_INTERNAL_API_PUBLIC_SIMPLE_METADATA_CHANGE_LIST_H_

#include <string>

#include "sync/api/metadata_change_list.h"
#include "sync/api/model_type_store.h"
#include "sync/base/sync_export.h"

namespace syncer_v2 {

// A MetadataChangeList implementation that is meant to be used in combination
// with a ModelTypeStore. It accumulates changes in member fields, and then when
// requested transfers them to the store/write batch.
class SYNC_EXPORT SimpleMetadataChangeList : public MetadataChangeList {
 public:
  SimpleMetadataChangeList();
  ~SimpleMetadataChangeList() override;

  void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) override;
  void ClearDataTypeState() override;
  void UpdateMetadata(const std::string& client_tag,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& client_tag) override;

  // Moves all currently accumulated changes into the write batch, clear out
  // local copies. Calling this multiple times will work, but should not be
  // necessary.
  void TranfserChanges(ModelTypeStore* store,
                       ModelTypeStore::WriteBatch* write_batch);
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_SIMPLE_METADATA_CHANGE_LIST_H_
