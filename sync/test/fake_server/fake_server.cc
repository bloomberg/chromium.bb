// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server.h"

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

using std::string;

// The parent tag for childen of the root node.
static const char kRootParentTag[] = "0";

// The default birthday value.
static const char kDefaultBirthday[] = "1234567890";

// The default keystore key.
static const char kDefaultKeystoreKey[] = "1111111111111111";

// A dummy value to use for the position_in_parent field of SyncEntity.
static const int64 kDummyBookmarkPositionInParent = 1337;

namespace syncer {
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

  // Determines whether the server should send |entity| to the client based
  // on its type and version.
  bool ClientWantsItem(const sync_pb::SyncEntity& entity) const {
    ModelTypeToVersionMap::const_iterator it =
        request_from_version_.find(GetModelType(entity));

    return it == request_from_version_.end() ?
        false : it->second < entity.version();
  }

  // Returns the mininum version seen across all types.
  int64 GetMinVersion() const {
    return min_version_;
  }

  // Returns the data type IDs of types being synced for the first time.
  std::vector<ModelType> GetFirstTimeTypes() const {
    std::vector<ModelType> types;

    ModelTypeToVersionMap::const_iterator it;
    for (it = request_from_version_.begin(); it != request_from_version_.end();
         ++it) {
      if (it->second == 0)
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

    ModelType model_type = GetModelTypeFromSpecificsFieldNumber(
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

FakeServer::~FakeServer() { }

void FakeServer::CreateDefaultPermanentItems(
    const std::vector<ModelType>& first_time_types) {
  for (std::vector<ModelType>::const_iterator it = first_time_types.begin();
       it != first_time_types.end(); ++it) {
    if (!ModelTypeSet::All().Has(*it)) {
      NOTREACHED() << "An unexpected ModelType was encountered.";
    }

    ModelType model_type = *it;
    CreateSyncEntity(model_type,
                     ModelTypeToRootTag(model_type),
                     ModelTypeToString(model_type),
                     kRootParentTag);

    if (model_type == BOOKMARKS) {
      CreateSyncEntity(BOOKMARKS,
                       "bookmark_bar",
                       "Bookmark Bar",
                       ModelTypeToRootTag(BOOKMARKS));
      CreateSyncEntity(BOOKMARKS,
                       "other_bookmarks",
                       "Other Bookmarks",
                       ModelTypeToRootTag(BOOKMARKS));
    }
  }


  // TODO(pvalenzuela): Create the mobile bookmarks folder when the fake server
  // is used by mobile tests.
}

void FakeServer::CreateSyncEntity(ModelType model_type,
                                  const std::string& id,
                                  const std::string& name,
                                  const std::string& parent_tag) {
  DCHECK(!id.empty());
  DCHECK(!name.empty());
  DCHECK(!parent_tag.empty());

  sync_pb::SyncEntity entity;
  entity.set_id_string(id);
  entity.set_non_unique_name(name);
  entity.set_name(name);
  entity.set_server_defined_unique_tag(id);
  entity.set_folder(true);
  entity.set_deleted(false);

  entity.set_parent_id_string(parent_tag);

  if (parent_tag != kRootParentTag && model_type == BOOKMARKS) {
    // Use a dummy value here.
    entity.set_position_in_parent(kDummyBookmarkPositionInParent);
  }

  sync_pb::EntitySpecifics* specifics = entity.mutable_specifics();
  AddDefaultFieldValue(model_type, specifics);

  SaveEntity(&entity);
}

void FakeServer::SaveEntity(sync_pb::SyncEntity* entity) {
  version_++;
  entity->set_version(version_);
  entity->set_sync_timestamp(version_);

  sync_pb::SyncEntity original_entity = entities_[entity->id_string()];
  entity->set_originator_cache_guid(original_entity.originator_cache_guid());
  entity->set_originator_client_item_id(
      original_entity.originator_client_item_id());

  entities_[entity->id_string()] = *entity;
}

int FakeServer::HandleCommand(const string& request,
                              int* response_code,
                              string* response) {
  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  DCHECK(parsed);

  sync_pb::ClientToServerResponse response_proto;
  switch (message.message_contents()) {
    case sync_pb::ClientToServerMessage::GET_UPDATES:
      response_proto = HandleGetUpdatesRequest(message);
      break;
    case sync_pb::ClientToServerMessage::COMMIT:
      response_proto = HandleCommitRequest(message);
      break;
    default:
      return net::ERR_NOT_IMPLEMENTED;
  }

  *response_code = net::HTTP_OK;
  *response = response_proto.SerializeAsString();
  return 0;
}

bool SyncEntityVersionComparator(const sync_pb::SyncEntity& first,
                                 const sync_pb::SyncEntity& second) {
  return first.version() < second.version();
}

sync_pb::ClientToServerResponse FakeServer::HandleGetUpdatesRequest(
    const sync_pb::ClientToServerMessage& message) {
  sync_pb::ClientToServerResponse response;
  response.set_error_code(sync_pb::SyncEnums::SUCCESS);
  response.set_store_birthday(birthday_);

  sync_pb::GetUpdatesResponse* get_updates_response =
      response.mutable_get_updates();
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  get_updates_response->set_changes_remaining(0);

  scoped_ptr<UpdateSieve> sieve = UpdateSieve::Create(message.get_updates());
  CreateDefaultPermanentItems(sieve->GetFirstTimeTypes());

  int64 min_version = sieve->GetMinVersion();

  bool send_encryption_keys_based_on_nigori = false;
  int64 max_response_version = 0;
  for (EntityMap::iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    sync_pb::SyncEntity entity = it->second;
    if (entity.version() > min_version && sieve->ClientWantsItem(entity)) {
      sync_pb::SyncEntity* response_entity =
          get_updates_response->add_entries();
      response_entity->CopyFrom(entity);
      max_response_version = std::max(max_response_version,
                                      response_entity->version());

      if (response_entity->name() == ModelTypeToString(NIGORI)) {
        send_encryption_keys_based_on_nigori =
            response_entity->specifics().nigori().passphrase_type() ==
                sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
      }
    }
  }

  if (send_encryption_keys_based_on_nigori ||
      message.get_updates().need_encryption_key()) {
    for (std::vector<std::string>::iterator it = keystore_keys_.begin();
         it != keystore_keys_.end(); ++it) {
      get_updates_response->add_encryption_keys(*it);
    }
  }

  sieve->UpdateProgressMarkers(max_response_version, get_updates_response);

  return response;
}

sync_pb::SyncEntity FakeServer::CommitEntity(const sync_pb::SyncEntity& entity,
                                             string guid) {
  // TODO(pvalenzuela): Implement this. Right now this method cheats and
  // doesn't actually commit.
  return entity;
}

sync_pb::ClientToServerResponse FakeServer::HandleCommitRequest(
    const sync_pb::ClientToServerMessage& message) {
  sync_pb::ClientToServerResponse response;
  response.set_error_code(sync_pb::SyncEnums::SUCCESS);
  response.set_store_birthday(birthday_);

  sync_pb::CommitMessage commit = message.commit();
  string guid = commit.cache_guid();

  sync_pb::CommitResponse* commit_response = response.mutable_commit();

  ::google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it;
  for (it = commit.entries().begin(); it != commit.entries().end(); ++it) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        commit_response->add_entryresponse();

    sync_pb::SyncEntity server_entity = CommitEntity(*it, guid);

    entry_response->set_id_string(server_entity.id_string());
    entry_response->set_response_type(sync_pb::CommitResponse::SUCCESS);
    entry_response->set_version(it->version() + 1);
  }

  return response;
}

}  // namespace syncer
