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
FakeServerEntity* UniqueClientEntity::Create(
    const sync_pb::SyncEntity& client_entity) {
  CHECK(client_entity.has_client_defined_unique_tag())
      << "A UniqueClientEntity must have a client-defined unique tag.";
  ModelType model_type =
      syncer::GetModelTypeFromSpecifics(client_entity.specifics());
  string id = EffectiveIdForClientTaggedEntity(client_entity);
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
std::string UniqueClientEntity::EffectiveIdForClientTaggedEntity(
    const sync_pb::SyncEntity& entity) {
  return FakeServerEntity::CreateId(
      syncer::GetModelTypeFromSpecifics(entity.specifics()),
      entity.client_defined_unique_tag());
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

void UniqueClientEntity::SerializeAsProto(sync_pb::SyncEntity* proto) {
  FakeServerEntity::SerializeBaseProtoFields(proto);

  sync_pb::EntitySpecifics* specifics = proto->mutable_specifics();
  specifics->CopyFrom(specifics_);

  proto->set_parent_id_string(GetParentId());
  proto->set_client_defined_unique_tag(client_defined_unique_tag_);
  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);
}

bool UniqueClientEntity::IsDeleted() const {
  return false;
}

bool UniqueClientEntity::IsFolder() const {
  return false;
}

}  // namespace fake_server
