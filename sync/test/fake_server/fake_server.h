// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"

namespace fake_server {

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
  typedef std::map<std::string, FakeServerEntity*> EntityMap;

  // Processes a GetUpdates call.
  bool HandleGetUpdatesRequest(const sync_pb::GetUpdatesMessage& get_updates,
                               sync_pb::GetUpdatesResponse* response);

  // Processes a Commit call.
  bool HandleCommitRequest(const sync_pb::CommitMessage& commit,
                           sync_pb::CommitResponse* response);

  // Inserts the appropriate permanent items in |entities_|.
  bool CreateDefaultPermanentItems(
      const std::vector<syncer::ModelType>& first_time_types);

  // Inserts the mobile bookmarks folder entity in |entities_|.
  bool CreateMobileBookmarksPermanentItem();

  // Saves a |entity| to |entities_|.
  void SaveEntity(FakeServerEntity* entity);

  // Commits a client-side SyncEntity to the server as a FakeServerEntity.
  // The client that sent the commit is identified via |client_guid| and all
  // entity ID renaming is tracked with |client_to_server_ids|. If the commit
  // is successful, true is returned and the server's version of the SyncEntity
  // is stored in |server_entity|.
  bool CommitEntity(const sync_pb::SyncEntity& client_entity,
                    sync_pb::CommitResponse_EntryResponse* entry_response,
                    std::string client_guid,
                    std::map<std::string, std::string>* client_to_server_ids);

  // Populates |entry_response| based on |entity|. It is assumed that
  // SaveEntity has already been called on |entity|.
  void BuildEntryResponseForSuccessfulCommit(
      sync_pb::CommitResponse_EntryResponse* entry_response,
      FakeServerEntity* entity);

  // Determines whether the SyncEntity with id_string |id| is a child of an
  // entity with id_string |potential_parent_id|.
  bool IsChild(const std::string& id, const std::string& potential_parent_id);

  // Creates and saves tombstones for all children of the entity with the given
  // |id|. A tombstone is not created for the entity itself.
  bool DeleteChildren(const std::string& id);

  // The lock used to ensure that only one client is communicating with server
  // at any given time.
  //
  // It is probably preferable to have FakeServer operate on its own thread and
  // communicate with it via PostTask, but clients would still need to wait for
  // requests to finish before proceeding.
  base::Lock lock_;

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

  // All ModelTypes for which permanent entities have been created. These types
  // are kept track of so that permanent entities are not recreated for new
  // clients.
  syncer::ModelTypeSet created_permanent_entity_types_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_
