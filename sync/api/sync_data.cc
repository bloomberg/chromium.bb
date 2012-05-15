// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_data.h"

#include <ostream>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "sync/internal_api/base_node.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/model_type.h"

void SyncData::ImmutableSyncEntityTraits::InitializeWrapper(
    Wrapper* wrapper) {
  *wrapper = new sync_pb::SyncEntity();
}

void SyncData::ImmutableSyncEntityTraits::DestroyWrapper(
    Wrapper* wrapper) {
  delete *wrapper;
}

const sync_pb::SyncEntity& SyncData::ImmutableSyncEntityTraits::Unwrap(
    const Wrapper& wrapper) {
  return *wrapper;
}

sync_pb::SyncEntity* SyncData::ImmutableSyncEntityTraits::UnwrapMutable(
    Wrapper* wrapper) {
  return *wrapper;
}

void SyncData::ImmutableSyncEntityTraits::Swap(sync_pb::SyncEntity* t1,
                                               sync_pb::SyncEntity* t2) {
  t1->Swap(t2);
}

SyncData::SyncData()
    : is_valid_(false),
      id_(sync_api::kInvalidId) {}

SyncData::SyncData(int64 id, sync_pb::SyncEntity* entity)
    : is_valid_(true),
      id_(id),
      immutable_entity_(entity) {}

SyncData::~SyncData() {}

// Static.
SyncData SyncData::CreateLocalDelete(
    const std::string& sync_tag,
    syncable::ModelType datatype) {
  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultFieldValue(datatype, &specifics);
  return CreateLocalData(sync_tag, "", specifics);
}

// Static.
SyncData SyncData::CreateLocalData(
    const std::string& sync_tag,
    const std::string& non_unique_title,
    const sync_pb::EntitySpecifics& specifics) {
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(sync_tag);
  entity.set_non_unique_name(non_unique_title);
  entity.mutable_specifics()->CopyFrom(specifics);
  return SyncData(sync_api::kInvalidId, &entity);
}

// Static.
SyncData SyncData::CreateRemoteData(
    int64 id, const sync_pb::EntitySpecifics& specifics) {
  DCHECK_NE(id, sync_api::kInvalidId);
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->CopyFrom(specifics);
  return SyncData(id, &entity);
}

bool SyncData::IsValid() const {
  return is_valid_;
}

const sync_pb::EntitySpecifics& SyncData::GetSpecifics() const {
  return immutable_entity_.Get().specifics();
}

syncable::ModelType SyncData::GetDataType() const {
  return syncable::GetModelTypeFromSpecifics(GetSpecifics());
}

const std::string& SyncData::GetTag() const {
  DCHECK(IsLocal());
  return immutable_entity_.Get().client_defined_unique_tag();
}

const std::string& SyncData::GetTitle() const {
  // TODO(zea): set this for data coming from the syncer too.
  DCHECK(immutable_entity_.Get().has_non_unique_name());
  return immutable_entity_.Get().non_unique_name();
}

int64 SyncData::GetRemoteId() const {
  DCHECK(!IsLocal());
  return id_;
}

bool SyncData::IsLocal() const {
  return id_ == sync_api::kInvalidId;
}

std::string SyncData::ToString() const {
  if (!IsValid())
    return "<Invalid SyncData>";

  std::string type = syncable::ModelTypeToString(GetDataType());
  std::string specifics;
  scoped_ptr<DictionaryValue> value(
      browser_sync::EntitySpecificsToValue(GetSpecifics()));
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &specifics);

  if (IsLocal()) {
    return "{ isLocal: true, type: " + type + ", tag: " + GetTag() +
        ", title: " + GetTitle() + ", specifics: " + specifics + "}";
  }

  std::string id = base::Int64ToString(GetRemoteId());
  return "{ isLocal: false, type: " + type + ", specifics: " + specifics +
      ", id: " + id + "}";
}

void PrintTo(const SyncData& sync_data, std::ostream* os) {
  *os << sync_data.ToString();
}
