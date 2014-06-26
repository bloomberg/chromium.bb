// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/unique_client_entity.h"

#include <string>

#include "base/basictypes.h"
#include "base/guid.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "sync/test/fake_server/permanent_entity.h"

using std::string;

using syncer::ModelType;

namespace fake_server {

UniqueClientEntity::~UniqueClientEntity() { }

// static
FakeServerEntity* UniqueClientEntity::CreateNew(
    const sync_pb::SyncEntity& client_entity) {
  CHECK(client_entity.has_client_defined_unique_tag())
      << "A UniqueClientEntity must have a client-defined unique tag.";
  ModelType model_type =
      syncer::GetModelTypeFromSpecifics(client_entity.specifics());
  string id = client_entity.version() == 0 ?
      FakeServerEntity::CreateId(model_type, base::GenerateGUID()) :
      client_entity.id_string();
  return new UniqueClientEntity(id,
                                model_type,
                                client_entity.version(),
                                client_entity.name(),
                                client_entity.client_defined_unique_tag(),
                                client_entity.specifics(),
                                client_entity.ctime(),
                                client_entity.mtime());
}

// static
FakeServerEntity* UniqueClientEntity::CreateUpdatedVersion(
    const sync_pb::SyncEntity& client_entity,
    FakeServerEntity* current_server_entity) {
  return new UniqueClientEntity(client_entity.id_string(),
                                current_server_entity->GetModelType(),
                                client_entity.version(),
                                client_entity.name(),
                                client_entity.client_defined_unique_tag(),
                                client_entity.specifics(),
                                client_entity.ctime(),
                                client_entity.mtime());
}

UniqueClientEntity::UniqueClientEntity(
    const string& id,
    const ModelType& model_type,
    int64 version,
    const string& name,
    const string& client_defined_unique_tag,
    const sync_pb::EntitySpecifics& specifics,
    int64 creation_time,
    int64 last_modified_time)
    : FakeServerEntity(id, model_type, version, name),
      client_defined_unique_tag_(client_defined_unique_tag),
      specifics_(specifics),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) { }

string UniqueClientEntity::GetParentId() const {
  // The parent ID for this type of entity should always be its ModelType's
  // root node.
  return FakeServerEntity::GetTopLevelId(model_type_);
}

sync_pb::SyncEntity* UniqueClientEntity::SerializeAsProto() {
  sync_pb::SyncEntity* sync_entity = new sync_pb::SyncEntity();
  FakeServerEntity::SerializeBaseProtoFields(sync_entity);

  sync_pb::EntitySpecifics* specifics = sync_entity->mutable_specifics();
  specifics->CopyFrom(specifics_);

  sync_entity->set_parent_id_string(GetParentId());
  sync_entity->set_client_defined_unique_tag(client_defined_unique_tag_);
  sync_entity->set_ctime(creation_time_);
  sync_entity->set_mtime(last_modified_time_);

  return sync_entity;
}

bool UniqueClientEntity::IsDeleted() const {
  return false;
}

bool UniqueClientEntity::IsFolder() const {
  return false;
}

}  // namespace fake_server
