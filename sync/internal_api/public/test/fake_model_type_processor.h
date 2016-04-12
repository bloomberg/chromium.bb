// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_PROCESSOR_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_PROCESSOR_H_

#include <memory>

#include "sync/internal_api/public/model_type_processor.h"

namespace syncer_v2 {

class FakeModelTypeProcessor : public ModelTypeProcessor {
 public:
  FakeModelTypeProcessor();
  ~FakeModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void OnCommitCompleted(const sync_pb::DataTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::DataTypeState& type_state,
                        const UpdateResponseDataList& updates) override;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_PROCESSOR_H_
