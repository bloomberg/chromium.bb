// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/unique_client_entity.h"

#include <string>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "sync/test/fake_server/permanent_entity.h"

using std::string;

using syncer::GetModelTypeFromSpecifics;
using syncer::ModelType;
using syncer::syncable::GenerateSyncableHash;

namespace fake_server {

namespace {

// A version must be passed when creating a FakeServerEntity, but this value
// is overrideen immediately when saving the entity in FakeServer.
const int64 kUnusedVersion = 0L;

// Default time (creation and last modified) used when creating entities.
const int64 kDefaultTime = 1234L;

}  // namespace

UniqueClientEntity::UniqueClientEntity(
    const string& id,
    ModelType model_type,
    int64 version,
    const string& name,
    const string& client_defined_unique_tag,
    const sync_pb::EntitySpecifics& specifics,
    int64 creation_time,
    int64 last_modified_time)
    : FakeServerEntity(id, model_type, version, name),
      client_defined_unique_tag_(client_defined_unique_tag),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {
  SetSpecifics(specifics);
}

UniqueClientEntity::~UniqueClientEntity() { }

// static
scoped_ptr<FakeServerEntity> UniqueClientEntity::Create(
    const sync_pb::SyncEntity& client_entity) {
  CHECK(client_entity.has_client_defined_unique_tag())
      << "A UniqueClientEntity must have a client-defined unique tag.";
  ModelType model_type =
      syncer::GetModelTypeFromSpecifics(client_entity.specifics());
  string id = EffectiveIdForClientTaggedEntity(client_entity);
  return scoped_ptr<FakeServerEntity>(new UniqueClientEntity(
      id, model_type, client_entity.version(), client_entity.name(),
      client_entity.client_defined_unique_tag(), client_entity.specifics(),
      client_entity.ctime(), client_entity.mtime()));
}

// static
scoped_ptr<FakeServerEntity> UniqueClientEntity::CreateForInjection(
    const string& name,
    const sync_pb::EntitySpecifics& entity_specifics) {
  ModelType model_type = GetModelTypeFromSpecifics(entity_specifics);
  string client_defined_unique_tag = GenerateSyncableHash(model_type, name);
  string id = FakeServerEntity::CreateId(model_type, client_defined_unique_tag);
  return scoped_ptr<FakeServerEntity>(
      new UniqueClientEntity(id,
                             model_type,
                             kUnusedVersion,
                             name,
                             client_defined_unique_tag,
                             entity_specifics,
                             kDefaultTime,
                             kDefaultTime));
}

// static
std::string UniqueClientEntity::EffectiveIdForClientTaggedEntity(
    const sync_pb::SyncEntity& entity) {
  return FakeServerEntity::CreateId(
      syncer::GetModelTypeFromSpecifics(entity.specifics()),
      entity.client_defined_unique_tag());
}

bool UniqueClientEntity::RequiresParentId() const {
  return false;
}

string UniqueClientEntity::GetParentId() const {
  // The parent ID for this type of entity should always be its ModelType's
  // root node.
  return FakeServerEntity::GetTopLevelId(GetModelType());
}

void UniqueClientEntity::SerializeAsProto(sync_pb::SyncEntity* proto) const {
  FakeServerEntity::SerializeBaseProtoFields(proto);

  proto->set_client_defined_unique_tag(client_defined_unique_tag_);
  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);
}

}  // namespace fake_server
