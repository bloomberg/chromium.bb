// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_METADATA_CHANGE_LIST_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_METADATA_CHANGE_LIST_H_

#include <string>
#include <vector>

#include "sync/api/metadata_change_list.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/entity_metadata.pb.h"

namespace syncer_v2 {

// A non-functional implementation of MetadataChangeList for
// testing purposes.
// This class simply records all calls with all arguments for further
// analysis by the test code.
class FakeMetadataChangeList : public MetadataChangeList {
 public:
  FakeMetadataChangeList();
  ~FakeMetadataChangeList() override;

  void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) override;
  void ClearDataTypeState() override;
  void UpdateMetadata(const std::string& client_tag,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& client_tag) override;

  enum Action {
    UPDATE_DATA_TYPE_STATE,
    CLEAR_DATA_TYPE_STATE,
    UPDATE_METADATA,
    CLEAR_METADATA
  };

  struct Record {
    Record();
    virtual ~Record();

    Action action;
    std::string tag;
    sync_pb::DataTypeState data_type_state;
    sync_pb::EntityMetadata metadata;
  };

  size_t GetNumRecords() const;
  const Record& GetNthRecord(size_t n) const;

 private:
  std::vector<Record> records_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_METADATA_CHANGE_LIST_H_
