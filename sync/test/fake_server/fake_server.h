// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// A fake version of the Sync server used for testing.
class FakeServer {
 public:
  FakeServer();
  virtual ~FakeServer();

  // Handles a /command POST to the server. If the return value is 0 (success),
  // |response_code| and |response| will be set. Otherwise, the return value
  // will be a network error code.
  int HandleCommand(const std::string& request,
                    int* response_code,
                    std::string* response);

 private:
  typedef std::map<std::string, sync_pb::SyncEntity> EntityMap;

  // Processes a GetUpdates call.
  sync_pb::ClientToServerResponse HandleGetUpdatesRequest(
      const sync_pb::ClientToServerMessage& message);

  // Processes a Commit call.
  sync_pb::ClientToServerResponse HandleCommitRequest(
      const sync_pb::ClientToServerMessage& message);

  // Inserts the appropriate permanent items in |entities_|.
  void CreateDefaultPermanentItems(
      const std::vector<ModelType>& first_time_types);

  // Creates and saves a SyncEntity of the given |model_type|. The entity's
  // id_string and server_defined_unique_tag fields are set to |id|. The
  // non_unique_name and name fields are set to |name|. Since tags are used as
  // ids, the parent_id_string field is set to |parent_tag|.
  void CreateSyncEntity(ModelType model_type,
                        const std::string& id,
                        const std::string& name,
                        const std::string& parent_tag);

  // Saves a |entity| to |entities_|.
  void SaveEntity(sync_pb::SyncEntity* entity);

  // Commits a client-side |entity| to |entities_|.
  sync_pb::SyncEntity CommitEntity(const sync_pb::SyncEntity& entity,
                                   std::string guid);

  // This is the last version number assigned to an entity. The next entity will
  // have a version number of version_ + 1.
  int64 version_;

  // The current birthday value.
  std::string birthday_;

  // All SyncEntity objects saved by the server. The key value is the entity's
  // id string.
  EntityMap entities_;

  // All Keystore keys known to the server.
  std::vector<std::string> keystore_keys_;
};

}  // namespace syncer

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_
