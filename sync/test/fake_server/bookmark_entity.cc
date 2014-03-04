// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/bookmark_entity.h"

#include <string>

#include "base/basictypes.h"
#include "base/guid.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"

using std::string;

using syncer::ModelType;

namespace fake_server {

BookmarkEntity::~BookmarkEntity() { }

// static
FakeServerEntity* BookmarkEntity::CreateNew(
    const sync_pb::SyncEntity& client_entity,
    const string& parent_id,
    const string& client_guid) {
  if (client_entity.version() != 0) {
    return NULL;
  }

  ModelType model_type =
      syncer::GetModelTypeFromSpecifics(client_entity.specifics());
  string id = FakeServerEntity::CreateId(model_type, base::GenerateGUID());
  string originator_cache_guid = client_guid;
  string originator_client_item_id = client_entity.id_string();

  return new BookmarkEntity(id,
                            model_type,
                            client_entity.version(),
                            client_entity.name(),
                            originator_cache_guid,
                            originator_client_item_id,
                            client_entity.unique_position(),
                            client_entity.specifics(),
                            client_entity.folder(),
                            parent_id,
                            client_entity.ctime(),
                            client_entity.mtime());
}

// static
FakeServerEntity* BookmarkEntity::CreateUpdatedVersion(
    const sync_pb::SyncEntity& client_entity,
    FakeServerEntity* current_server_entity,
    const string& parent_id) {
  if (client_entity.version() == 0 || current_server_entity == NULL) {
    return NULL;
  }

  BookmarkEntity* current_bookmark_entity =
      static_cast<BookmarkEntity*>(current_server_entity);
  string originator_cache_guid =
      current_bookmark_entity->originator_cache_guid_;
  string originator_client_item_id =
      current_bookmark_entity->originator_client_item_id_;
  ModelType model_type =
      syncer::GetModelTypeFromSpecifics(client_entity.specifics());

 return new BookmarkEntity(client_entity.id_string(),
                           model_type,
                           client_entity.version(),
                           client_entity.name(),
                           originator_cache_guid,
                           originator_client_item_id,
                           client_entity.unique_position(),
                           client_entity.specifics(),
                           client_entity.folder(),
                           parent_id,
                           client_entity.ctime(),
                           client_entity.mtime());
}

BookmarkEntity::BookmarkEntity(
    const string& id,
    const ModelType& model_type,
    int64 version,
    const string& name,
    const string& originator_cache_guid,
    const string& originator_client_item_id,
    const sync_pb::UniquePosition& unique_position,
    const sync_pb::EntitySpecifics& specifics,
    bool is_folder,
    const string& parent_id,
    int64 creation_time,
    int64 last_modified_time)
    : FakeServerEntity(id, model_type, version, name),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id),
      unique_position_(unique_position),
      specifics_(specifics),
      is_folder_(is_folder),
      parent_id_(parent_id),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) { }

string BookmarkEntity::GetParentId() const {
  return parent_id_;
}

sync_pb::SyncEntity* BookmarkEntity::SerializeAsProto() {
  sync_pb::SyncEntity* sync_entity = new sync_pb::SyncEntity();
  FakeServerEntity::SerializeBaseProtoFields(sync_entity);

  sync_pb::EntitySpecifics* specifics = sync_entity->mutable_specifics();
  specifics->CopyFrom(specifics_);

  sync_entity->set_originator_cache_guid(originator_cache_guid_);
  sync_entity->set_originator_client_item_id(originator_client_item_id_);

  sync_entity->set_parent_id_string(parent_id_);
  sync_entity->set_ctime(creation_time_);
  sync_entity->set_mtime(last_modified_time_);

  sync_pb::UniquePosition* unique_position =
      sync_entity->mutable_unique_position();
  unique_position->CopyFrom(unique_position_);

  return sync_entity;
}

bool BookmarkEntity::IsDeleted() const {
  return false;
}

bool BookmarkEntity::IsFolder() const {
  return is_folder_;
}

}  // namespace fake_server
