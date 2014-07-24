// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server.h"

#include <algorithm>
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

using syncer::GetModelType;
using syncer::ModelType;
using syncer::ModelTypeSet;

// The default store birthday value.
static const char kDefaultStoreBirthday[] = "1234567890";

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

  // Returns the minimum version seen across all types.
  int64 GetMinVersion() const {
    return min_version_;
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
  CHECK_GT(get_updates_message.from_progress_marker_size(), 0)
      << "A GetUpdates request must have at least one progress marker.";

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
      CHECK(parsed) << "Unable to parse progress marker token.";
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

FakeServer::FakeServer() : version_(0),
                           store_birthday_(kDefaultStoreBirthday),
                           authenticated_(true),
                           error_type_(sync_pb::SyncEnums::SUCCESS) {
  keystore_keys_.push_back(kDefaultKeystoreKey);
  CHECK(CreateDefaultPermanentItems());
}

FakeServer::~FakeServer() {
  STLDeleteContainerPairSecondPointers(entities_.begin(), entities_.end());
}

bool FakeServer::CreateDefaultPermanentItems() {
  ModelTypeSet all_types = syncer::ProtocolTypes();
  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    ModelType model_type = it.Get();
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

void FakeServer::HandleCommand(const string& request,
                               const HandleCommandCallback& callback) {
  if (!authenticated_) {
    callback.Run(0, net::HTTP_UNAUTHORIZED, string());
    return;
  }

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  CHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  sync_pb::SyncEnums_ErrorType error_code;
  sync_pb::ClientToServerResponse response_proto;

  if (message.has_store_birthday() &&
      message.store_birthday() != store_birthday_) {
    error_code = sync_pb::SyncEnums::NOT_MY_BIRTHDAY;
  } else if (error_type_ != sync_pb::SyncEnums::SUCCESS) {
    error_code = error_type_;
  } else {
    bool success = false;
    switch (message.message_contents()) {
      case sync_pb::ClientToServerMessage::GET_UPDATES:
        success = HandleGetUpdatesRequest(message.get_updates(),
                                          response_proto.mutable_get_updates());
        break;
      case sync_pb::ClientToServerMessage::COMMIT:
        success = HandleCommitRequest(message.commit(),
                                      message.invalidator_client_id(),
                                      response_proto.mutable_commit());
        break;
      default:
        callback.Run(net::ERR_NOT_IMPLEMENTED, 0, string());;
        return;
    }

    if (!success) {
      // TODO(pvalenzuela): Add logging here so that tests have more info about
      // the failure.
      callback.Run(net::ERR_FAILED, 0, string());
      return;
    }

    error_code = sync_pb::SyncEnums::SUCCESS;
  }

  response_proto.set_error_code(error_code);
  response_proto.set_store_birthday(store_birthday_);
  callback.Run(0, net::HTTP_OK, response_proto.SerializeAsString());
}

bool FakeServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    sync_pb::GetUpdatesResponse* response) {
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  response->set_changes_remaining(0);

  scoped_ptr<UpdateSieve> sieve = UpdateSieve::Create(get_updates);

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

string FakeServer::CommitEntity(
    const sync_pb::SyncEntity& client_entity,
    sync_pb::CommitResponse_EntryResponse* entry_response,
    string client_guid,
    string parent_id) {
  if (client_entity.version() == 0 && client_entity.deleted()) {
    return string();
  }

  FakeServerEntity* entity;
  if (client_entity.deleted()) {
    entity = TombstoneEntity::Create(client_entity.id_string());
    // TODO(pvalenzuela): Change the behavior of DeleteChilden so that it does
    // not modify server data if it fails.
    if (!DeleteChildren(client_entity.id_string())) {
      return string();
    }
  } else if (GetModelType(client_entity) == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    entity = PermanentEntity::CreateUpdatedNigoriEntity(
        client_entity,
        entities_[client_entity.id_string()]);
  } else if (client_entity.has_client_defined_unique_tag()) {
    entity = UniqueClientEntity::Create(client_entity);
  } else {
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
    return string();
  }

  SaveEntity(entity);
  BuildEntryResponseForSuccessfulCommit(entry_response, entity);
  return entity->GetId();
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
    const std::string& invalidator_client_id,
    sync_pb::CommitResponse* response) {
  std::map<string, string> client_to_server_ids;
  string guid = commit.cache_guid();
  ModelTypeSet committed_model_types;

  // TODO(pvalenzuela): Add validation of CommitMessage.entries.
  ::google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it;
  for (it = commit.entries().begin(); it != commit.entries().end(); ++it) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->add_entryresponse();

    sync_pb::SyncEntity client_entity = *it;
    string parent_id = client_entity.parent_id_string();
    if (client_to_server_ids.find(parent_id) !=
        client_to_server_ids.end()) {
      parent_id = client_to_server_ids[parent_id];
    }

    string entity_id = CommitEntity(client_entity,
                                    entry_response,
                                    guid,
                                    parent_id);
    if (entity_id.empty()) {
      return false;
    }

    // Record the ID if it was renamed.
    if (entity_id != client_entity.id_string()) {
      client_to_server_ids[client_entity.id_string()] = entity_id;
    }
    FakeServerEntity* entity = entities_[entity_id];
    committed_model_types.Put(entity->GetModelType());
  }

  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCommit(invalidator_client_id, committed_model_types));
  return true;
}

scoped_ptr<base::DictionaryValue> FakeServer::GetEntitiesAsDictionaryValue() {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());

  // Initialize an empty ListValue for all ModelTypes.
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    dictionary->Set(ModelTypeToString(it.Get()), new base::ListValue());
  }

  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    FakeServerEntity* entity = it->second;
    if (entity->IsDeleted() || entity->IsFolder()) {
      // Tombstones are ignored as they don't represent current data. Folders
      // are also ignored as current verification infrastructure does not
      // consider them.
      continue;
    }
    base::ListValue* list_value;
    if (!dictionary->GetList(ModelTypeToString(entity->GetModelType()),
                                               &list_value)) {
      return scoped_ptr<base::DictionaryValue>();
    }
    // TODO(pvalenzuela): Store more data for each entity so additional
    // verification can be performed. One example of additional verification
    // is checking the correctness of the bookmark hierarchy.
    list_value->Append(new base::StringValue(entity->GetName()));
  }

  return dictionary.Pass();
}

void FakeServer::InjectEntity(scoped_ptr<FakeServerEntity> entity) {
  SaveEntity(entity.release());
}

bool FakeServer::SetNewStoreBirthday(const string& store_birthday) {
  if (store_birthday_ == store_birthday)
    return false;

  store_birthday_ = store_birthday;
  return true;
}

void FakeServer::SetAuthenticated() {
  authenticated_ = true;
}

void FakeServer::SetUnauthenticated() {
  authenticated_ = false;
}

// TODO(pvalenzuela): comments from Richard: we should look at
// mock_connection_manager.cc and take it as a warning. This style of injecting
// errors works when there's one or two conditions we care about, but it can
// eventually lead to a hairball once we have many different conditions and
// triggering logic.
void FakeServer::TriggerError(const sync_pb::SyncEnums::ErrorType& error_type) {
  error_type_ = error_type;
}

void FakeServer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeServer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace fake_server
