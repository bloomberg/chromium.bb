// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server.h"

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/bookmark_entity.h"
#include "sync/test/fake_server/permanent_entity.h"
#include "sync/test/fake_server/tombstone_entity.h"
#include "sync/test/fake_server/unique_client_entity.h"

using std::string;
using std::vector;

using base::AutoLock;
using syncer::GetModelType;
using syncer::ModelType;

// The default birthday value.
static const char kDefaultBirthday[] = "1234567890";

// The default keystore key.
static const char kDefaultKeystoreKey[] = "1111111111111111";

namespace fake_server {

class FakeServerEntity;

namespace {

// A filter used during GetUpdates calls to determine what information to
// send back to the client. There is a 1:1 correspondence between any given
// GetUpdates call and an UpdateSieve instance.
class UpdateSieve {
 public:
  ~UpdateSieve() { }

  // Factory method for creating an UpdateSieve.
  static scoped_ptr<UpdateSieve> Create(
      const sync_pb::GetUpdatesMessage& get_updates_message);

  // Sets the progress markers in |get_updates_response| given the progress
  // markers from the original GetUpdatesMessage and |new_version| (the latest
  // version in the entries sent back).
  void UpdateProgressMarkers(
      int64 new_version,
      sync_pb::GetUpdatesResponse* get_updates_response) const {
    ModelTypeToVersionMap::const_iterator it;
    for (it = request_from_version_.begin(); it != request_from_version_.end();
         ++it) {
      sync_pb::DataTypeProgressMarker* new_marker =
          get_updates_response->add_new_progress_marker();
      new_marker->set_data_type_id(
          GetSpecificsFieldNumberFromModelType(it->first));

      int64 version = std::max(new_version, it->second);
      new_marker->set_token(base::Int64ToString(version));
    }
  }

  // Determines whether the server should send an |entity| to the client as
  // part of a GetUpdatesResponse.
  bool ClientWantsItem(FakeServerEntity* entity) const {
    int64 version = entity->GetVersion();
    if (version <= min_version_) {
      return false;
    } else if (entity->IsDeleted()) {
      return true;
    }

    ModelTypeToVersionMap::const_iterator it =
        request_from_version_.find(entity->GetModelType());

    return it == request_from_version_.end() ? false : it->second < version;
  }

  // Returns the mininum version seen across all types.
  int64 GetMinVersion() const {
    return min_version_;
  }

  // Returns the data type IDs of types being synced for the first time.
  vector<ModelType> GetFirstTimeTypes(
      syncer::ModelTypeSet created_permanent_entity_types) const {
    vector<ModelType> types;

    ModelTypeToVersionMap::const_iterator it;
    for (it = request_from_version_.begin(); it != request_from_version_.end();
         ++it) {
      if (it->second == 0 && !created_permanent_entity_types.Has(it->first))
        types.push_back(it->first);
    }

    return types;
  }

 private:
  typedef std::map<ModelType, int64> ModelTypeToVersionMap;

  // Creates an UpdateSieve.
  UpdateSieve(const ModelTypeToVersionMap request_from_version,
              const int64 min_version)
      : request_from_version_(request_from_version),
        min_version_(min_version) { }

  // Maps data type IDs to the latest version seen for that type.
  const ModelTypeToVersionMap request_from_version_;

  // The minimum version seen among all data types.
  const int min_version_;
};

scoped_ptr<UpdateSieve> UpdateSieve::Create(
    const sync_pb::GetUpdatesMessage& get_updates_message) {
  DCHECK_GT(get_updates_message.from_progress_marker_size(), 0);

  UpdateSieve::ModelTypeToVersionMap request_from_version;
  int64 min_version = std::numeric_limits<int64>::max();
  for (int i = 0; i < get_updates_message.from_progress_marker_size(); i++) {
    sync_pb::DataTypeProgressMarker marker =
        get_updates_message.from_progress_marker(i);

    int64 version = 0;
    // Let the version remain zero if there is no token or an empty token (the
    // first request for this type).
    if (marker.has_token() && !marker.token().empty()) {
      bool parsed = base::StringToInt64(marker.token(), &version);
      DCHECK(parsed);
    }

    ModelType model_type = syncer::GetModelTypeFromSpecificsFieldNumber(
        marker.data_type_id());
    request_from_version[model_type] = version;

    if (version < min_version)
      min_version = version;
  }

  return scoped_ptr<UpdateSieve>(
      new UpdateSieve(request_from_version, min_version));
}

}  // namespace

FakeServer::FakeServer() : version_(0), birthday_(kDefaultBirthday) {
  keystore_keys_.push_back(kDefaultKeystoreKey);
}

FakeServer::~FakeServer() {
  STLDeleteContainerPairSecondPointers(entities_.begin(), entities_.end());
}

bool FakeServer::CreateDefaultPermanentItems(
    const vector<ModelType>& first_time_types) {
  for (vector<ModelType>::const_iterator it = first_time_types.begin();
       it != first_time_types.end(); ++it) {
    if (!syncer::ModelTypeSet::All().Has(*it)) {
      NOTREACHED() << "An unexpected ModelType was encountered.";
    }

    ModelType model_type = *it;
    if (created_permanent_entity_types_.Has(model_type)) {
      // Do not create an entity for the type if it has already been created.
      continue;
    }

    FakeServerEntity* top_level_entity =
        PermanentEntity::CreateTopLevel(model_type);
    if (top_level_entity == NULL) {
      return false;
    }
    SaveEntity(top_level_entity);

    if (model_type == syncer::BOOKMARKS) {
      FakeServerEntity* bookmark_bar_entity =
          PermanentEntity::Create(syncer::BOOKMARKS,
                                  "bookmark_bar",
                                  "Bookmark Bar",
                                  ModelTypeToRootTag(syncer::BOOKMARKS));
      if (bookmark_bar_entity == NULL) {
        return false;
      }
      SaveEntity(bookmark_bar_entity);

      FakeServerEntity* other_bookmarks_entity =
          PermanentEntity::Create(syncer::BOOKMARKS,
                                  "other_bookmarks",
                                  "Other Bookmarks",
                                  ModelTypeToRootTag(syncer::BOOKMARKS));
      if (other_bookmarks_entity == NULL) {
        return false;
      }
      SaveEntity(other_bookmarks_entity);
    }

    created_permanent_entity_types_.Put(model_type);
  }

  return true;
}

bool FakeServer::CreateMobileBookmarksPermanentItem() {
  // This folder is called "Synced Bookmarks" by sync and is renamed
  // "Mobile Bookmarks" by the mobile client UIs.
  FakeServerEntity* mobile_bookmarks_entity =
      PermanentEntity::Create(syncer::BOOKMARKS,
                              "synced_bookmarks",
                              "Synced Bookmarks",
                              ModelTypeToRootTag(syncer::BOOKMARKS));
  if (mobile_bookmarks_entity == NULL) {
    return false;
  }
  SaveEntity(mobile_bookmarks_entity);
  return true;
}

void FakeServer::SaveEntity(FakeServerEntity* entity) {
  delete entities_[entity->GetId()];
  entity->SetVersion(++version_);
  entities_[entity->GetId()] = entity;
}

int FakeServer::HandleCommand(const string& request,
                              int* response_code,
                              string* response) {
  AutoLock lock(lock_);

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  DCHECK(parsed);

  sync_pb::ClientToServerResponse response_proto;
  bool success;
  switch (message.message_contents()) {
    case sync_pb::ClientToServerMessage::GET_UPDATES:
      success = HandleGetUpdatesRequest(message.get_updates(),
                                        response_proto.mutable_get_updates());
      break;
    case sync_pb::ClientToServerMessage::COMMIT:
      success = HandleCommitRequest(message.commit(),
                                    response_proto.mutable_commit());
      break;
    default:
      return net::ERR_NOT_IMPLEMENTED;
  }

  if (!success) {
    // TODO(pvalenzuela): Add logging here so that tests have more info about
    // the failure.
    return net::HTTP_BAD_REQUEST;
  }

  response_proto.set_error_code(sync_pb::SyncEnums::SUCCESS);
  response_proto.set_store_birthday(birthday_);
  *response_code = net::HTTP_OK;
  *response = response_proto.SerializeAsString();
  return 0;
}

bool FakeServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    sync_pb::GetUpdatesResponse* response) {
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  response->set_changes_remaining(0);

  scoped_ptr<UpdateSieve> sieve = UpdateSieve::Create(get_updates);
  vector<ModelType> first_time_types = sieve->GetFirstTimeTypes(
      created_permanent_entity_types_);
  if (!CreateDefaultPermanentItems(first_time_types)) {
    return false;
  }
  if (get_updates.create_mobile_bookmarks_folder() &&
      !CreateMobileBookmarksPermanentItem()) {
    return false;
  }

  bool send_encryption_keys_based_on_nigori = false;
  int64 max_response_version = 0;
  for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    FakeServerEntity* entity = it->second;
    if (sieve->ClientWantsItem(entity)) {
      sync_pb::SyncEntity* response_entity = response->add_entries();
      response_entity->CopyFrom(*(entity->SerializeAsProto()));
      max_response_version = std::max(max_response_version,
                                      response_entity->version());

      if (entity->GetModelType() == syncer::NIGORI) {
        send_encryption_keys_based_on_nigori =
            response_entity->specifics().nigori().passphrase_type() ==
                sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
      }
    }
  }

  if (send_encryption_keys_based_on_nigori ||
      get_updates.need_encryption_key()) {
    for (vector<string>::iterator it = keystore_keys_.begin();
         it != keystore_keys_.end(); ++it) {
      response->add_encryption_keys(*it);
    }
  }

  sieve->UpdateProgressMarkers(max_response_version, response);
  return true;
}

bool FakeServer::CommitEntity(
    const sync_pb::SyncEntity& client_entity,
    sync_pb::CommitResponse_EntryResponse* entry_response,
    string client_guid,
    std::map<string, string>* client_to_server_ids) {
  if (client_entity.version() == 0 && client_entity.deleted()) {
    return false;
  }

  FakeServerEntity* entity;
  if (client_entity.deleted()) {
    entity = TombstoneEntity::Create(client_entity.id_string());
    // TODO(pvalenzuela): Change the behavior of DeleteChilden so that it does
    // not modify server data if it fails.
    if (!DeleteChildren(client_entity.id_string())) {
      return false;
    }
  } else if (GetModelType(client_entity) == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    entity = PermanentEntity::CreateUpdatedNigoriEntity(
        client_entity,
        entities_[client_entity.id_string()]);
  } else if (client_entity.has_client_defined_unique_tag()) {
    if (entities_.find(client_entity.id_string()) != entities_.end()) {
      entity = UniqueClientEntity::CreateUpdatedVersion(
          client_entity,
          entities_[client_entity.id_string()]);
    } else {
      entity = UniqueClientEntity::CreateNew(client_entity);
    }
  } else {
    string parent_id = client_entity.parent_id_string();
    if (client_to_server_ids->find(parent_id) !=
        client_to_server_ids->end()) {
      parent_id = (*client_to_server_ids)[parent_id];
    }
    // TODO(pvalenzuela): Validate entity's parent ID.
    if (entities_.find(client_entity.id_string()) != entities_.end()) {
      entity = BookmarkEntity::CreateUpdatedVersion(
        client_entity,
        entities_[client_entity.id_string()],
        parent_id);
    } else {
      entity = BookmarkEntity::CreateNew(client_entity, parent_id, client_guid);
    }
  }

  if (entity == NULL) {
    // TODO(pvalenzuela): Add logging so that it is easier to determine why
    // creation failed.
    return false;
  }

  // Record the ID if it was renamed.
  if (client_entity.id_string() != entity->GetId()) {
    (*client_to_server_ids)[client_entity.id_string()] = entity->GetId();
  }

  SaveEntity(entity);
  BuildEntryResponseForSuccessfulCommit(entry_response, entity);
  return true;
}

void FakeServer::BuildEntryResponseForSuccessfulCommit(
  sync_pb::CommitResponse_EntryResponse* entry_response,
  FakeServerEntity* entity) {
    entry_response->set_response_type(sync_pb::CommitResponse::SUCCESS);
    entry_response->set_id_string(entity->GetId());

    if (entity->IsDeleted()) {
      entry_response->set_version(entity->GetVersion() + 1);
    } else {
      entry_response->set_version(entity->GetVersion());
      entry_response->set_name(entity->GetName());
    }
}

bool FakeServer::IsChild(const string& id, const string& potential_parent_id) {
  if (entities_.find(id) == entities_.end()) {
    // We've hit an ID (probably the imaginary root entity) that isn't stored
    // by the server, so it can't be a child.
    return false;
  } else if (entities_[id]->GetParentId() == potential_parent_id) {
    return true;
  } else {
    // Recursively look up the tree.
    return IsChild(entities_[id]->GetParentId(), potential_parent_id);
  }
}

bool FakeServer::DeleteChildren(const string& id) {
  vector<string> child_ids;
  for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    if (IsChild(it->first, id)) {
      child_ids.push_back(it->first);
    }
  }

  for (vector<string>::iterator it = child_ids.begin(); it != child_ids.end();
       ++it) {
    FakeServerEntity* tombstone = TombstoneEntity::Create(*it);
    if (tombstone == NULL) {
      LOG(WARNING) << "Tombstone creation failed for entity with ID " << *it;
      return false;
    }
    SaveEntity(tombstone);
  }

  return true;
}

bool FakeServer::HandleCommitRequest(
    const sync_pb::CommitMessage& commit,
    sync_pb::CommitResponse* response) {
  std::map<string, string> client_to_server_ids;
  string guid = commit.cache_guid();

  // TODO(pvalenzuela): Add validation of CommitMessage.entries.
  ::google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it;
  for (it = commit.entries().begin(); it != commit.entries().end(); ++it) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->add_entryresponse();

    if (!CommitEntity(*it, entry_response, guid, &client_to_server_ids)) {
      return false;
    }
  }

  return true;
}

}  // namespace fake_server
