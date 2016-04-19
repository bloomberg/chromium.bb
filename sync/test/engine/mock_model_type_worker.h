// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_WORKER_H_
#define SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_WORKER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "sync/engine/commit_queue.h"
#include "sync/internal_api/public/model_type_processor.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/data_type_state.pb.h"

namespace syncer_v2 {

// Receives and records commit requests sent through the ModelTypeWorker.
//
// This class also includes features intended to help mock out server behavior.
// It has some basic functionality to keep track of server state and generate
// plausible UpdateResponseData and CommitResponseData messages.
class MockModelTypeWorker : public CommitQueue {
 public:
  MockModelTypeWorker(const sync_pb::DataTypeState& data_type_state,
                      ModelTypeProcessor* processor);
  ~MockModelTypeWorker() override;

  // Implementation of ModelTypeWorker.
  void EnqueueForCommit(const CommitRequestDataList& list) override;

  // Getters to inspect the requests sent to this object.
  size_t GetNumPendingCommits() const;
  CommitRequestDataList GetNthPendingCommit(size_t n) const;
  bool HasPendingCommitForTag(const std::string& tag) const;
  CommitRequestData GetLatestPendingCommitForTag(const std::string& tag) const;

  // Expect that the |n|th commit request list has one commit request for |tag|
  // with |value| set.
  void ExpectNthPendingCommit(size_t n,
                              const std::string& tag,
                              const std::string& value);

  // For each tag in |tags|, expect a corresponding request list of length one.
  void ExpectPendingCommits(const std::vector<std::string>& tags);

  // Trigger an update from the server containing a single entity. See
  // GenerateUpdateData for parameter descriptions. |version_offset| defaults to
  // 1 and |ekn| defaults to the current encryption key name the worker has.
  void UpdateFromServer(const std::string& tag, const std::string& value);
  void UpdateFromServer(const std::string& tag,
                        const std::string& value,
                        int64_t version_offset);
  void UpdateFromServer(const std::string& tag,
                        const std::string& value,
                        int64_t version_offset,
                        const std::string& ekn);

  // Returns an UpdateResponseData representing an update received from
  // the server. Updates server state accordingly. |tag| is used to generate the
  // tag hash and the specifics along with |value|.
  //
  // The |version_offset| field can be used to emulate stale data (ie. versions
  // going backwards), reflections and redeliveries (ie. several instances of
  // the same version) or new updates.
  //
  // |ekn| is the encryption key name this item will fake having.
  UpdateResponseData GenerateUpdateData(const std::string& tag,
                                        const std::string& value,
                                        int64_t version_offset,
                                        const std::string& ekn);

  // Triggers a server-side deletion of the entity with |tag|; updates server
  // state accordingly.
  void TombstoneFromServer(const std::string& tag);

  // Pops one pending commit from the front of the queue and send a commit
  // response to the processor for it.
  void AckOnePendingCommit();

  // Set the encryption key to |ekn| and inform the processor with an update
  // containing the data in |update|, which defaults to an empty list.
  void UpdateWithEncryptionKey(const std::string& ekn);
  void UpdateWithEncryptionKey(const std::string& ekn,
                               const UpdateResponseDataList& update);

 private:
  // Generate an ID string.
  static std::string GenerateId(const std::string& tag_hash);

  // Returns a commit response that indicates a successful commit of the
  // given |request_data|. Updates server state accordingly.
  CommitResponseData SuccessfulCommitResponse(
      const CommitRequestData& request_data);

  // Retrieve or set the server version.
  int64_t GetServerVersion(const std::string& tag_hash);
  void SetServerVersion(const std::string& tag_hash, int64_t version);

  sync_pb::DataTypeState data_type_state_;

  // A pointer to the processor for this mock worker.
  ModelTypeProcessor* processor_;

  // A record of past commits requests.
  std::deque<CommitRequestDataList> pending_commits_;

  // Map of versions by client tag.
  // This is an essential part of the mocked server state.
  std::map<const std::string, int64_t> server_versions_;

  DISALLOW_COPY_AND_ASSIGN(MockModelTypeWorker);
};

}  // namespace syncer_v2

#endif  // SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_WORKER_H_
