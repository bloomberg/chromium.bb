// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "sync/internal_api/public/test/fake_metadata_change_list.h"

namespace syncer_v2 {

FakeMetadataChangeList::FakeMetadataChangeList() {}

FakeMetadataChangeList::~FakeMetadataChangeList() {}

FakeMetadataChangeList::Record::Record() {}

FakeMetadataChangeList::Record::~Record() {}

void FakeMetadataChangeList::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {
  Record record;
  record.action = UPDATE_DATA_TYPE_STATE;
  record.data_type_state = data_type_state;
  records_.push_back(record);
}

void FakeMetadataChangeList::ClearDataTypeState() {
  Record record;
  record.action = CLEAR_DATA_TYPE_STATE;
  records_.push_back(record);
}

void FakeMetadataChangeList::UpdateMetadata(
    const std::string& client_tag,
    const sync_pb::EntityMetadata& metadata) {
  Record record;
  record.action = UPDATE_METADATA;
  record.tag = client_tag;
  record.metadata.CopyFrom(metadata);
  records_.push_back(record);
}

void FakeMetadataChangeList::ClearMetadata(const std::string& client_tag) {
  Record record;
  record.action = CLEAR_METADATA;
  record.tag = client_tag;
  records_.push_back(record);
}

size_t FakeMetadataChangeList::GetNumRecords() const {
  return records_.size();
}

const FakeMetadataChangeList::Record& FakeMetadataChangeList::GetNthRecord(
    size_t n) const {
  return records_[n];
}

}  // namespace syncer_v2
