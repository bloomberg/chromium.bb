// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_SYNC_PROXY_H_
#define SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_SYNC_PROXY_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "sync/engine/model_type_sync_proxy.h"
#include "sync/engine/non_blocking_sync_common.h"

namespace syncer {

// Mocks the ModelTypeSyncProxy.
//
// This mock is made simpler by not using any threads.  It does still have the
// ability to defer execution if we need to test race conditions, though.
//
// It maintains some state to try to make its behavior more realistic.  It
// updates this state as it creates commit requests or receives update and
// commit responses.
//
// It keeps a log of all received messages so tests can make assertions based
// on their value.
class MockModelTypeSyncProxy : public ModelTypeSyncProxy {
 public:
  MockModelTypeSyncProxy();
  virtual ~MockModelTypeSyncProxy();

  // Implementation of ModelTypeSyncProxy.
  virtual void OnCommitCompleted(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list) OVERRIDE;
  virtual void OnUpdateReceived(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list) OVERRIDE;

  // By default, this object behaves as if all messages are processed
  // immediately.  Sometimes it is useful to defer work until later, as might
  // happen in the real world if the model thread's task queue gets backed up.
  void SetSynchronousExecution(bool is_synchronous);

  // Runs any work that was deferred while this class was in asynchronous mode.
  //
  // This function is not useful unless this object is set to synchronous
  // execution mode, which is the default.
  void RunQueuedTasks();

  // Generate commit or deletion requests to be sent to the server.
  // These functions update local state to keep sequence numbers consistent.
  //
  // A real ModelTypeSyncProxy would forward these kinds of messages
  // directly to its attached ModelTypeSyncWorker.  These methods
  // return the value to the caller so the test framework can handle them as it
  // sees fit.
  CommitRequestData CommitRequest(const std::string& tag_hash,
                                  const sync_pb::EntitySpecifics& specifics);
  CommitRequestData DeleteRequest(const std::string& tag_hash);

  // Getters to access the log of received update responses.
  //
  // Does not includes repsonses that are in pending tasks.
  size_t GetNumUpdateResponses() const;
  UpdateResponseDataList GetNthUpdateResponse(size_t n) const;
  DataTypeState GetNthTypeStateReceivedInUpdateResponse(size_t n) const;

  // Getters to access the log of received commit responses.
  //
  // Does not includes repsonses that are in pending tasks.
  size_t GetNumCommitResponses() const;
  CommitResponseDataList GetNthCommitResponse(size_t n) const;
  DataTypeState GetNthTypeStateReceivedInCommitResponse(size_t n) const;

  // Getters to access the lastest update response for a given tag_hash.
  bool HasUpdateResponse(const std::string& tag_hash) const;
  UpdateResponseData GetUpdateResponse(const std::string& tag_hash) const;

  // Getters to access the lastest commit response for a given tag_hash.
  bool HasCommitResponse(const std::string& tag_hash) const;
  CommitResponseData GetCommitResponse(const std::string& tag_hash) const;

 private:
  // Process a received commit response.
  //
  // Implemented as an Impl method so we can defer its execution in some cases.
  void OnCommitCompletedImpl(const DataTypeState& type_state,
                             const CommitResponseDataList& response_list);

  // Process a received update response.
  //
  // Implemented as an Impl method so we can defer its execution in some cases.
  void OnUpdateReceivedImpl(const DataTypeState& type_state,
                            const UpdateResponseDataList& response_list);

  // Getter and setter for per-item sequence number tracking.
  int64 GetCurrentSequenceNumber(const std::string& tag_hash) const;
  int64 GetNextSequenceNumber(const std::string& tag_hash);

  // Getter and setter for per-item base version tracking.
  int64 GetBaseVersion(const std::string& tag_hash) const;
  void SetBaseVersion(const std::string& tag_hash, int64 version);

  // Getters and setter for server-assigned ID values.
  bool HasServerAssignedId(const std::string& tag_hash) const;
  const std::string& GetServerAssignedId(const std::string& tag_hash) const;
  void SetServerAssignedId(const std::string& tag_hash, const std::string& id);

  // State related to the implementation of deferred work.
  // See SetSynchronousExecution() for details.
  bool is_synchronous_;
  std::vector<base::Closure> pending_tasks_;

  // A log of messages received by this object.
  std::vector<CommitResponseDataList> received_commit_responses_;
  std::vector<UpdateResponseDataList> received_update_responses_;
  std::vector<DataTypeState> type_states_received_on_update_;
  std::vector<DataTypeState> type_states_received_on_commit_;

  // Latest responses received, indexed by tag_hash.
  std::map<const std::string, CommitResponseData> commit_response_items_;
  std::map<const std::string, UpdateResponseData> update_response_items_;

  // The per-item state maps.
  std::map<const std::string, int64> sequence_numbers_;
  std::map<const std::string, int64> base_versions_;
  std::map<const std::string, std::string> assigned_ids_;

  DISALLOW_COPY_AND_ASSIGN(MockModelTypeSyncProxy);
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_MOCK_MODEL_TYPE_SYNC_PROXY_H_
