// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/simple_metadata_change_list.h"

namespace syncer_v2 {

SimpleMetadataChangeList::SimpleMetadataChangeList() {}

SimpleMetadataChangeList::~SimpleMetadataChangeList() {}

void SimpleMetadataChangeList::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {
  // TODO(skym): Implementation.
}

void SimpleMetadataChangeList::UpdateMetadata(
    const std::string& client_tag,
    const sync_pb::EntityMetadata& metadata) {
  // TODO(skym): Implementation.
}

void SimpleMetadataChangeList::ClearDataTypeState() {
  // TODO(skym): Implementation.
}

void SimpleMetadataChangeList::ClearMetadata(const std::string& client_tag) {
  // TODO(skym): Implementation.
}

void SimpleMetadataChangeList::TranfserChanges(
    ModelTypeStore* store,
    ModelTypeStore::WriteBatch* write_batch) {
  // TODO(skym): Implementation.
}

}  // namespace syncer_v2
