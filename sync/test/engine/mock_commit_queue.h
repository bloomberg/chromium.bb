// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_MOCK_COMMIT_QUEUE_H_
#define SYNC_TEST_ENGINE_MOCK_COMMIT_QUEUE_H_

#include <vector>

#include "base/macros.h"
#include "sync/engine/commit_queue.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"

namespace syncer_v2 {

// Receives and records commit requests sent through the CommitQueue.
//
// This class also includes features intended to help mock out server behavior.
// It has some basic functionality to keep track of server state and generate
// plausible UpdateResponseData and CommitResponseData messages.
class MockCommitQueue : public CommitQueue {
 public:
  MockCommitQueue();
  ~MockCommitQueue() override;

  // Implementation of CommitQueue.
  void EnqueueForCommit(const CommitRequestDataList& list) override;

  // Getters to inspect the requests sent to this object.
  size_t GetNumCommitRequestLists() const;
  CommitRequestDataList GetNthCommitRequestList(size_t n) const;
  bool HasCommitRequestForTagHash(const std::string& tag_hash) const;
  CommitRequestData GetLatestCommitRequestForTagHash(
      const std::string& tag_hash) const;

  // Functions to produce state as though it came from a real server and had
  // been filtered through a real CommitQueue.

  // Returns an UpdateResponseData representing an update received from
  // the server.  Updates server state accordingly.
  //
  // The |version_offset| field can be used to emulate stale data (ie. versions
  // going backwards), reflections and redeliveries (ie. several instances of
  // the same version) or new updates.
  UpdateResponseData UpdateFromServer(
      int64 version_offset,
      const std::string& tag_hash,
      const sync_pb::EntitySpecifics& specifics);

  // Returns an UpdateResponseData representing a tombstone update from the
  // server.  Updates server state accordingly.
  UpdateResponseData TombstoneFromServer(int64 version_offset,
                                         const std::string& tag_hash);

  // Returns a commit response that indicates a successful commit of the
  // given |request_data|.  Updates server state accordingly.
  CommitResponseData SuccessfulCommitResponse(
      const CommitRequestData& request_data);

  // Sets the encryption key name used for updates from the server.
  // (ie. the key other clients are using to encrypt their commits.)
  // The default value is an empty string, which indicates no encryption.
  void SetServerEncryptionKey(const std::string& key_name);

 private:
  // Generate an ID string.
  static std::string GenerateId(const std::string& tag_hash);

  // Retrieve or set the server version.
  int64 GetServerVersion(const std::string& tag_hash);
  void SetServerVersion(const std::string& tag_hash, int64 version);

  // A record of past commits requests.
  std::vector<CommitRequestDataList> commit_request_lists_;

  // Map of versions by client tag.
  // This is an essential part of the mocked server state.
  std::map<const std::string, int64> server_versions_;

  // Name of the encryption key in use on other clients.
  std::string server_encryption_key_name_;

  DISALLOW_COPY_AND_ASSIGN(MockCommitQueue);
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_MOCK_COMMIT_QUEUE_H_
