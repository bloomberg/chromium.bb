// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/metadata_batch.h"

namespace syncer_v2 {

MetadataBatch::MetadataBatch() {}
MetadataBatch::~MetadataBatch() {}

EntityMetadataMap&& MetadataBatch::TakeAllMetadata() {
  return std::move(metadata_map_);
}

void MetadataBatch::AddMetadata(const std::string& client_tag,
                                const sync_pb::EntityMetadata& metadata) {
  metadata_map_.insert(std::make_pair(client_tag, metadata));
}

const sync_pb::DataTypeState& MetadataBatch::GetDataTypeState() const {
  return state_;
}

void MetadataBatch::SetDataTypeState(const sync_pb::DataTypeState& state) {
  state_ = state;
}

}  // namespace syncer_v2
