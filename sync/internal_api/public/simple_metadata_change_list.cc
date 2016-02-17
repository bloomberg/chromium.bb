// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/simple_metadata_change_list.h"

namespace syncer_v2 {

SimpleMetadataChangeList::SimpleMetadataChangeList() {}

SimpleMetadataChangeList::~SimpleMetadataChangeList() {}

void SimpleMetadataChangeList::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {
  state_change_.reset(new DataTypeStateChange{UPDATE, data_type_state});
}

void SimpleMetadataChangeList::ClearDataTypeState() {
  state_change_.reset(new DataTypeStateChange{CLEAR});
}

void SimpleMetadataChangeList::UpdateMetadata(
    const std::string& client_tag,
    const sync_pb::EntityMetadata& metadata) {
  metadata_changes_[client_tag] = {UPDATE, metadata};
}

void SimpleMetadataChangeList::ClearMetadata(const std::string& client_tag) {
  metadata_changes_[client_tag] = {CLEAR, sync_pb::EntityMetadata()};
}

const SimpleMetadataChangeList::MetadataChanges&
SimpleMetadataChangeList::GetMetadataChanges() const {
  return metadata_changes_;
}

bool SimpleMetadataChangeList::HasDataTypeStateChange() const {
  return state_change_.get() != nullptr;
}

const SimpleMetadataChangeList::DataTypeStateChange&
SimpleMetadataChangeList::GetDataTypeStateChange() const {
  return *state_change_.get();
}

void SimpleMetadataChangeList::TransferChanges(
    ModelTypeStore* store,
    ModelTypeStore::WriteBatch* write_batch) {
  // TODO(skym): Implementation.
}

}  // namespace syncer_v2
