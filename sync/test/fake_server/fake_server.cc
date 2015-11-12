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
using syncer::GetModelTypeFromSpecifics;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace fake_server {

class FakeServerEntity;

namespace {

// The default keystore key.
static const char kDefaultKeystoreKey[] = "1111111111111111";

// Properties of the bookmark bar permanent folder.
static const char kBookmarkBarFolderServerTag[] = "bookmark_bar";
static const char kBookmarkBarFolderName[] = "Bookmark Bar";

// Properties of the other bookmarks permanent folder.
static const char kOtherBookmarksFolderServerTag[] =  "other_bookmarks";
static const char kOtherBookmarksFolderName[] = "Other Bookmarks";

// Properties of the synced bookmarks permanent folder.
static const char kSyncedBookmarksFolderServerTag[] = "synced_bookmarks";
static const char kSyncedBookmarksFolderName[] = "Synced Bookmarks";

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
  bool ClientWantsItem(const FakeServerEntity& entity) const {
    int64 version = entity.GetVersion();
    if (version <= min_version_) {
      return false;
    } else if (entity.IsDeleted()) {
      return true;
    }

    ModelTypeToVersionMap::const_iterator it =
        request_from_version_.find(entity.GetModelType());

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

// Returns whether |entity| is deleted or permanent.
bool IsDeletedOrPermanent(const FakeServerEntity& entity) {
  return entity.IsDeleted() || entity.IsPermanent();
}

// This is a hack to add the parent ID string if it has not been set inside
// FakeServer::SerializeAsProto (this method only adds the parent ID to the
// proto if FakeServerEntity::RequiresParentId returns true).
//
// In the case where FakeServer::enable_implicit_permanent_folder_creation_ is
// true, we must add the parent ID to the proto.
//
// TODO(pvalenzuela): Remove this when creation of explicit, non-required
// permanent folders is no longer supported. When this feature transition
// happens, these non-required parent IDs will no longer be necessary.
void AddParentIdIfNecessary(const FakeServerEntity& entity,
                            sync_pb::SyncEntity* entity_proto) {
  if (!entity_proto->has_parent_id_string())
    entity_proto->set_parent_id_string(entity.GetParentId());
}

}  // namespace

FakeServer::FakeServer() : version_(0),
                           store_birthday_(0),
                           authenticated_(true),
                           error_type_(sync_pb::SyncEnums::SUCCESS),
                           alternate_triggered_errors_(false),
                           request_counter_(0),
                           network_enabled_(true),
                           enable_implicit_permanent_folder_creation_(false),
                           weak_ptr_factory_(this) {
  keystore_keys_.push_back(kDefaultKeystoreKey);

  const bool create_result = CreateDefaultPermanentItems();
  DCHECK(create_result) << "Permanent items were not created successfully.";
}

FakeServer::~FakeServer() {}

bool FakeServer::CreatePermanentBookmarkFolder(const std::string& server_tag,
                                               const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<FakeServerEntity> entity =
      PermanentEntity::Create(syncer::BOOKMARKS, server_tag, name,
                              ModelTypeToRootTag(syncer::BOOKMARKS));
  if (!entity)
    return false;

  SaveEntity(entity.Pass());
  return true;
}

bool FakeServer::CreateDefaultPermanentItems() {
  // Permanent folders are always required for Bookmarks (hierarchical
  // structure) and Nigori (data stored in permanent root folder).
  ModelTypeSet permanent_folder_types =
      enable_implicit_permanent_folder_creation_ ?
      ModelTypeSet(syncer::BOOKMARKS, syncer::NIGORI) : syncer::ProtocolTypes();

  for (ModelTypeSet::Iterator it = permanent_folder_types.First(); it.Good();
       it.Inc()) {
    ModelType model_type = it.Get();

    scoped_ptr<FakeServerEntity> top_level_entity =
        PermanentEntity::CreateTopLevel(model_type);
    if (!top_level_entity) {
      return false;
    }
    SaveEntity(top_level_entity.Pass());

    if (model_type == syncer::BOOKMARKS) {
      if (!CreatePermanentBookmarkFolder(kBookmarkBarFolderServerTag,
                                         kBookmarkBarFolderName))
        return false;
      if (!CreatePermanentBookmarkFolder(kOtherBookmarksFolderServerTag,
                                         kOtherBookmarksFolderName))
        return false;
    }
  }

  return true;
}

void FakeServer::UpdateEntityVersion(FakeServerEntity* entity) {
  entity->SetVersion(++version_);
}

void FakeServer::SaveEntity(scoped_ptr<FakeServerEntity> entity) {
  UpdateEntityVersion(entity.get());
  const string id = entity->GetId();
  entities_.set(id, entity.Pass());
}

void FakeServer::HandleCommand(const string& request,
                               const base::Closure& completion_closure,
                               int* error_code,
                               int* response_code,
                               std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_enabled_) {
    *error_code = net::ERR_FAILED;
    *response_code = net::ERR_FAILED;
    *response = string();
    completion_closure.Run();
    return;
  }
  request_counter_++;

  if (!authenticated_) {
    *error_code = 0;
    *response_code = net::HTTP_UNAUTHORIZED;
    *response = string();
    completion_closure.Run();
    return;
  }

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  CHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  sync_pb::ClientToServerResponse response_proto;

  if (message.has_store_birthday() &&
      message.store_birthday() != GetStoreBirthday()) {
    response_proto.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  } else if (error_type_ != sync_pb::SyncEnums::SUCCESS &&
             ShouldSendTriggeredError()) {
    response_proto.set_error_code(error_type_);
  } else if (triggered_actionable_error_.get() && ShouldSendTriggeredError()) {
    sync_pb::ClientToServerResponse_Error* error =
        response_proto.mutable_error();
    error->CopyFrom(*(triggered_actionable_error_.get()));
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
      case sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA:
        ClearServerData();
        response_proto.mutable_clear_server_data();
        success = true;
        break;
      default:
        *error_code = net::ERR_NOT_IMPLEMENTED;
        *response_code = 0;
        *response = string();
        completion_closure.Run();
        return;
    }

    if (!success) {
      // TODO(pvalenzuela): Add logging here so that tests have more info about
      // the failure.
      *error_code = net::ERR_FAILED;
      *response_code = 0;
      *response = string();
      completion_closure.Run();
      return;
    }

    response_proto.set_error_code(sync_pb::SyncEnums::SUCCESS);
  }

  response_proto.set_store_birthday(GetStoreBirthday());

  *error_code = 0;
  *response_code = net::HTTP_OK;
  *response = response_proto.SerializeAsString();
  completion_closure.Run();
}

bool FakeServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    sync_pb::GetUpdatesResponse* response) {
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  response->set_changes_remaining(0);

  scoped_ptr<UpdateSieve> sieve = UpdateSieve::Create(get_updates);

  // This folder is called "Synced Bookmarks" by sync and is renamed
  // "Mobile Bookmarks" by the mobile client UIs.
  if (get_updates.create_mobile_bookmarks_folder() &&
      !CreatePermanentBookmarkFolder(kSyncedBookmarksFolderServerTag,
                                     kSyncedBookmarksFolderName)) {
    return false;
  }

  bool send_encryption_keys_based_on_nigori = false;
  int64 max_response_version = 0;
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const FakeServerEntity& entity = *it->second;
    if (sieve->ClientWantsItem(entity)) {
      sync_pb::SyncEntity* response_entity = response->add_entries();
      entity.SerializeAsProto(response_entity);
      if (!enable_implicit_permanent_folder_creation_)
        AddParentIdIfNecessary(entity, response_entity);

      max_response_version = std::max(max_response_version,
                                      response_entity->version());

      if (entity.GetModelType() == syncer::NIGORI) {
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
    const string& client_guid,
    const string& parent_id) {
  if (client_entity.version() == 0 && client_entity.deleted()) {
    return string();
  }

  scoped_ptr<FakeServerEntity> entity;
  if (client_entity.deleted()) {
    entity = TombstoneEntity::Create(client_entity.id_string());
    DeleteChildren(client_entity.id_string());
  } else if (GetModelType(client_entity) == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    CHECK(iter != entities_.end());
    entity = PermanentEntity::CreateUpdatedNigoriEntity(client_entity,
                                                        *iter->second);
  } else if (client_entity.has_client_defined_unique_tag()) {
    entity = UniqueClientEntity::Create(client_entity);
  } else {
    // TODO(pvalenzuela): Validate entity's parent ID.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    if (iter != entities_.end()) {
      entity = BookmarkEntity::CreateUpdatedVersion(client_entity,
                                                    *iter->second, parent_id);
    } else {
      entity = BookmarkEntity::CreateNew(client_entity, parent_id, client_guid);
    }
  }

  if (!entity) {
    // TODO(pvalenzuela): Add logging so that it is easier to determine why
    // creation failed.
    return string();
  }

  const std::string id = entity->GetId();
  SaveEntity(entity.Pass());
  BuildEntryResponseForSuccessfulCommit(id, entry_response);
  return id;
}

void FakeServer::BuildEntryResponseForSuccessfulCommit(
    const std::string& entity_id,
    sync_pb::CommitResponse_EntryResponse* entry_response) {
  EntityMap::const_iterator iter = entities_.find(entity_id);
  CHECK(iter != entities_.end());
  const FakeServerEntity& entity = *iter->second;
  entry_response->set_response_type(sync_pb::CommitResponse::SUCCESS);
  entry_response->set_id_string(entity.GetId());

  if (entity.IsDeleted()) {
    entry_response->set_version(entity.GetVersion() + 1);
  } else {
    entry_response->set_version(entity.GetVersion());
    entry_response->set_name(entity.GetName());
  }
}

bool FakeServer::IsChild(const string& id, const string& potential_parent_id) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end()) {
    // We've hit an ID (probably the imaginary root entity) that isn't stored
    // by the server, so it can't be a child.
    return false;
  }

  const FakeServerEntity& entity = *iter->second;
  if (entity.GetParentId() == potential_parent_id)
    return true;

  // Recursively look up the tree.
  return IsChild(entity.GetParentId(), potential_parent_id);
}

void FakeServer::DeleteChildren(const string& id) {
  std::set<string> tombstones_ids;
  // Find all the children of id.
  for (auto& entity : entities_) {
    if (IsChild(entity.first, id)) {
      tombstones_ids.insert(entity.first);
    }
  }

  for (auto& tombstone_id : tombstones_ids) {
    SaveEntity(TombstoneEntity::Create(tombstone_id));
  }
}

bool FakeServer::HandleCommitRequest(const sync_pb::CommitMessage& commit,
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

    const string entity_id =
        CommitEntity(client_entity, entry_response, guid, parent_id);
    if (entity_id.empty()) {
      return false;
    }

    // Record the ID if it was renamed.
    if (entity_id != client_entity.id_string()) {
      client_to_server_ids[client_entity.id_string()] = entity_id;
    }

    EntityMap::const_iterator iter = entities_.find(entity_id);
    CHECK(iter != entities_.end());
    committed_model_types.Put(iter->second->GetModelType());
  }

  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCommit(invalidator_client_id, committed_model_types));
  return true;
}

scoped_ptr<base::DictionaryValue> FakeServer::GetEntitiesAsDictionaryValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());

  // Initialize an empty ListValue for all ModelTypes.
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    dictionary->Set(ModelTypeToString(it.Get()), new base::ListValue());
  }

  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const FakeServerEntity& entity = *it->second;
    if (IsDeletedOrPermanent(entity)) {
      // Tombstones are ignored as they don't represent current data. Folders
      // are also ignored as current verification infrastructure does not
      // consider them.
      continue;
    }
    base::ListValue* list_value;
    if (!dictionary->GetList(ModelTypeToString(entity.GetModelType()),
                             &list_value)) {
      return scoped_ptr<base::DictionaryValue>();
    }
    // TODO(pvalenzuela): Store more data for each entity so additional
    // verification can be performed. One example of additional verification
    // is checking the correctness of the bookmark hierarchy.
    list_value->Append(new base::StringValue(entity.GetName()));
  }

  return dictionary.Pass();
}

std::vector<sync_pb::SyncEntity> FakeServer::GetSyncEntitiesByModelType(
    ModelType model_type) {
  std::vector<sync_pb::SyncEntity> sync_entities;
  DCHECK(thread_checker_.CalledOnValidThread());
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const FakeServerEntity& entity = *it->second;
    if (!IsDeletedOrPermanent(entity) && entity.GetModelType() == model_type) {
      sync_pb::SyncEntity sync_entity;
      entity.SerializeAsProto(&sync_entity);
      if (!enable_implicit_permanent_folder_creation_)
        AddParentIdIfNecessary(entity, &sync_entity);
      sync_entities.push_back(sync_entity);
    }
  }
  return sync_entities;
}

void FakeServer::InjectEntity(scoped_ptr<FakeServerEntity> entity) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SaveEntity(entity.Pass());
}

bool FakeServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->GetModelType() !=
          GetModelTypeFromSpecifics(updated_specifics)) {
    return false;
  }

  scoped_ptr<FakeServerEntity> entity = entities_.take_and_erase(iter);
  entity->SetSpecifics(updated_specifics);
  UpdateEntityVersion(entity.get());
  entities_.insert(id, entity.Pass());
  return true;
}

bool FakeServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->GetModelType() != syncer::BOOKMARKS ||
      GetModelTypeFromSpecifics(updated_specifics) != syncer::BOOKMARKS) {
    return false;
  }

  scoped_ptr<BookmarkEntity> entity(
      static_cast<BookmarkEntity*>(entities_.take_and_erase(iter).release()));

  entity->SetParentId(parent_id);
  entity->SetSpecifics(updated_specifics);
  if (updated_specifics.has_bookmark()) {
    entity->SetName(updated_specifics.bookmark().title());
  }
  UpdateEntityVersion(entity.get());
  entities_.insert(id, entity.Pass());
  return true;
}

void FakeServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  entities_.clear();
  keystore_keys_.clear();
  ++store_birthday_;
}

void FakeServer::SetAuthenticated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  authenticated_ = true;
}

void FakeServer::SetUnauthenticated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  authenticated_ = false;
}

bool FakeServer::TriggerError(const sync_pb::SyncEnums::ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (triggered_actionable_error_.get()) {
    DVLOG(1) << "Only one type of error can be triggered at any given time.";
    return false;
  }

  error_type_ = error_type;
  return true;
}

bool FakeServer::TriggerActionableError(
    const sync_pb::SyncEnums::ErrorType& error_type,
    const string& description,
    const string& url,
    const sync_pb::SyncEnums::Action& action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ != sync_pb::SyncEnums::SUCCESS) {
    DVLOG(1) << "Only one type of error can be triggered at any given time.";
    return false;
  }

  sync_pb::ClientToServerResponse_Error* error =
      new sync_pb::ClientToServerResponse_Error();
  error->set_error_type(error_type);
  error->set_error_description(description);
  error->set_url(url);
  error->set_action(action);
  triggered_actionable_error_.reset(error);
  return true;
}

bool FakeServer::EnableAlternatingTriggeredErrors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ == sync_pb::SyncEnums::SUCCESS &&
      !triggered_actionable_error_.get()) {
    DVLOG(1) << "No triggered error set. Alternating can't be enabled.";
    return false;
  }

  alternate_triggered_errors_ = true;
  // Reset the counter so that the the first request yields a triggered error.
  request_counter_ = 0;
  return true;
}

bool FakeServer::ShouldSendTriggeredError() const {
  if (!alternate_triggered_errors_)
    return true;

  // Check that the counter is odd so that we trigger an error on the first
  // request after alternating is enabled.
  return request_counter_ % 2 != 0;
}

void FakeServer::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void FakeServer::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void FakeServer::EnableNetwork() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_enabled_ = true;
}

void FakeServer::DisableNetwork() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_enabled_ = false;
}

std::string FakeServer::GetBookmarkBarFolderId() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    FakeServerEntity* entity = it->second;
    if (entity->GetName() == kBookmarkBarFolderName &&
        entity->IsFolder() &&
        entity->GetModelType() == syncer::BOOKMARKS) {
      return entity->GetId();
    }
  }
  NOTREACHED() << "Bookmark Bar entity not found.";
  return "";
}

void FakeServer::EnableImplicitPermanentFolderCreation() {
  enable_implicit_permanent_folder_creation_ = true;
}

base::WeakPtr<FakeServer> FakeServer::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

std::string FakeServer::GetStoreBirthday() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Int64ToString(store_birthday_);
}

}  // namespace fake_server
