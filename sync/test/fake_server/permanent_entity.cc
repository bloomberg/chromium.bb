// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/permanent_entity.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"

using std::string;

using syncer::ModelType;

// The parent tag for children of the root entity. Entities with this parent are
// referred to as top level enities.
static const char kRootParentTag[] = "0";

namespace fake_server {

PermanentEntity::~PermanentEntity() { }

// static
FakeServerEntity* PermanentEntity::Create(const ModelType& model_type,
                                          const string& server_tag,
                                          const string& name,
                                          const string& parent_server_tag) {
  CHECK(model_type != syncer::UNSPECIFIED) << "The entity's ModelType is "
                                           << "invalid.";
  CHECK(!server_tag.empty()) << "A PermanentEntity must have a server tag.";
  CHECK(!name.empty()) << "The entity must have a non-empty name.";
  CHECK(!parent_server_tag.empty()) << "A PermanentEntity must have a parent "
                                    << "server tag.";
  CHECK(parent_server_tag != kRootParentTag) << "Top-level entities should not "
                                             << "be created with this factory.";

  string id = FakeServerEntity::CreateId(model_type, server_tag);
  string parent_id = FakeServerEntity::CreateId(model_type, parent_server_tag);
  sync_pb::EntitySpecifics entity_specifics;
  AddDefaultFieldValue(model_type, &entity_specifics);
  return new PermanentEntity(id,
                             model_type,
                             name,
                             parent_id,
                             server_tag,
                             entity_specifics);
}

// static
FakeServerEntity* PermanentEntity::CreateTopLevel(const ModelType& model_type) {
  CHECK(model_type != syncer::UNSPECIFIED) << "The entity's ModelType is "
                                           << "invalid.";
  string server_tag = syncer::ModelTypeToRootTag(model_type);
  string name = syncer::ModelTypeToString(model_type);
  string id = FakeServerEntity::GetTopLevelId(model_type);
  sync_pb::EntitySpecifics entity_specifics;
  AddDefaultFieldValue(model_type, &entity_specifics);
  return new PermanentEntity(id,
                             model_type,
                             name,
                             kRootParentTag,
                             server_tag,
                             entity_specifics);
}

// static
FakeServerEntity* PermanentEntity::CreateUpdatedNigoriEntity(
    const sync_pb::SyncEntity& client_entity,
    FakeServerEntity* current_server_entity) {
  ModelType model_type = current_server_entity->GetModelType();
  CHECK(model_type == syncer::NIGORI) << "This factory only supports NIGORI "
                                      << "entities.";

  return new PermanentEntity(current_server_entity->GetId(),
                             model_type,
                             current_server_entity->GetName(),
                             current_server_entity->GetParentId(),
                             syncer::ModelTypeToRootTag(model_type),
                             client_entity.specifics());
}

PermanentEntity::PermanentEntity(const string& id,
                                 const ModelType& model_type,
                                 const string& name,
                                 const string& parent_id,
                                 const string& server_defined_unique_tag,
                                 const sync_pb::EntitySpecifics& specifics)
    : FakeServerEntity(id, model_type, 0, name),
      server_defined_unique_tag_(server_defined_unique_tag),
      parent_id_(parent_id),
      specifics_(specifics) { }

string PermanentEntity::GetParentId() const {
  return parent_id_;
}

void PermanentEntity::SerializeAsProto(sync_pb::SyncEntity* proto) {
  FakeServerEntity::SerializeBaseProtoFields(proto);

  sync_pb::EntitySpecifics* specifics = proto->mutable_specifics();
  specifics->CopyFrom(specifics_);

  proto->set_parent_id_string(parent_id_);
  proto->set_server_defined_unique_tag(server_defined_unique_tag_);
}

bool PermanentEntity::IsDeleted() const {
  return false;
}

bool PermanentEntity::IsFolder() const {
  return true;
}

}  // namespace fake_server
