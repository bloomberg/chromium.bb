// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_model_type_processor.h"

#include "sync/engine/commit_queue.h"

namespace syncer_v2 {

FakeModelTypeProcessor::FakeModelTypeProcessor() {}
FakeModelTypeProcessor::~FakeModelTypeProcessor() {}

void FakeModelTypeProcessor::ConnectSync(std::unique_ptr<CommitQueue> worker) {}

void FakeModelTypeProcessor::DisconnectSync() {}

void FakeModelTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& response_list) {}

void FakeModelTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    const UpdateResponseDataList& updates) {}

}  // namespace syncer_v2
