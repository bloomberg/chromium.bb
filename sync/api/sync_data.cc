// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_data.h"

#include <ostream>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"

namespace {

sync_pb::AttachmentIdProto IdToProto(
    const syncer::AttachmentId& attachment_id) {
  return attachment_id.GetProto();
}

syncer::AttachmentId ProtoToId(const sync_pb::AttachmentIdProto& proto) {
  return syncer::AttachmentId::CreateFromProto(proto);
}

// Return true iff |attachment_ids| contains duplicates.
bool ContainsDuplicateAttachments(
    const syncer::AttachmentIdList& attachment_ids) {
  syncer::AttachmentIdSet id_set;
  id_set.insert(attachment_ids.begin(), attachment_ids.end());
  return id_set.size() != attachment_ids.size();
}

}  // namespace

namespace syncer {

void SyncData::ImmutableSyncEntityTraits::InitializeWrapper(Wrapper* wrapper) {
  *wrapper = new sync_pb::SyncEntity();
}

void SyncData::ImmutableSyncEntityTraits::DestroyWrapper(Wrapper* wrapper) {
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

SyncData::SyncData() : id_(kInvalidId), is_valid_(false) {}

SyncData::SyncData(int64 id,
                   sync_pb::SyncEntity* entity,
                   const base::Time& remote_modification_time,
                   const syncer::AttachmentServiceProxy& attachment_service)
    : id_(id),
      remote_modification_time_(remote_modification_time),
      immutable_entity_(entity),
      attachment_service_(attachment_service),
      is_valid_(true) {}

SyncData::~SyncData() {}

// Static.
SyncData SyncData::CreateLocalDelete(const std::string& sync_tag,
                                     ModelType datatype) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(datatype, &specifics);
  return CreateLocalData(sync_tag, std::string(), specifics);
}

// Static.
SyncData SyncData::CreateLocalData(const std::string& sync_tag,
                                   const std::string& non_unique_title,
                                   const sync_pb::EntitySpecifics& specifics) {
  syncer::AttachmentIdList attachment_ids;
  return CreateLocalDataWithAttachments(
      sync_tag, non_unique_title, specifics, attachment_ids);
}

// Static.
SyncData SyncData::CreateLocalDataWithAttachments(
    const std::string& sync_tag,
    const std::string& non_unique_title,
    const sync_pb::EntitySpecifics& specifics,
    const AttachmentIdList& attachment_ids) {
  DCHECK(!ContainsDuplicateAttachments(attachment_ids));
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(sync_tag);
  entity.set_non_unique_name(non_unique_title);
  entity.mutable_specifics()->CopyFrom(specifics);
  std::transform(attachment_ids.begin(),
                 attachment_ids.end(),
                 RepeatedFieldBackInserter(entity.mutable_attachment_id()),
                 IdToProto);
  return SyncData(kInvalidId,
                  &entity,
                  base::Time(),
                  AttachmentServiceProxy());
}

// Static.
SyncData SyncData::CreateRemoteData(
    int64 id,
    const sync_pb::EntitySpecifics& specifics,
    const base::Time& modification_time,
    const AttachmentIdList& attachment_ids,
    const AttachmentServiceProxy& attachment_service) {
  DCHECK_NE(id, kInvalidId);
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->CopyFrom(specifics);
  std::transform(attachment_ids.begin(),
                 attachment_ids.end(),
                 RepeatedFieldBackInserter(entity.mutable_attachment_id()),
                 IdToProto);
  return SyncData(id, &entity, modification_time, attachment_service);
}

bool SyncData::IsValid() const { return is_valid_; }

const sync_pb::EntitySpecifics& SyncData::GetSpecifics() const {
  return immutable_entity_.Get().specifics();
}

ModelType SyncData::GetDataType() const {
  return GetModelTypeFromSpecifics(GetSpecifics());
}

const std::string& SyncData::GetTitle() const {
  // TODO(zea): set this for data coming from the syncer too.
  DCHECK(immutable_entity_.Get().has_non_unique_name());
  return immutable_entity_.Get().non_unique_name();
}

bool SyncData::IsLocal() const { return id_ == kInvalidId; }

std::string SyncData::ToString() const {
  if (!IsValid())
    return "<Invalid SyncData>";

  std::string type = ModelTypeToString(GetDataType());
  std::string specifics;
  scoped_ptr<base::DictionaryValue> value(
      EntitySpecificsToValue(GetSpecifics()));
  base::JSONWriter::WriteWithOptions(
      value.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &specifics);

  if (IsLocal()) {
    SyncDataLocal sync_data_local(*this);
    return "{ isLocal: true, type: " + type + ", tag: " +
           sync_data_local.GetTag() + ", title: " + GetTitle() +
           ", specifics: " + specifics + "}";
  }

  SyncDataRemote sync_data_remote(*this);
  std::string id = base::Int64ToString(sync_data_remote.GetId());
  return "{ isLocal: false, type: " + type + ", specifics: " + specifics +
         ", id: " + id + "}";
}

void PrintTo(const SyncData& sync_data, std::ostream* os) {
  *os << sync_data.ToString();
}

AttachmentIdList SyncData::GetAttachmentIds() const {
  AttachmentIdList result;
  const sync_pb::SyncEntity& entity = immutable_entity_.Get();
  std::transform(entity.attachment_id().begin(),
                 entity.attachment_id().end(),
                 std::back_inserter(result),
                 ProtoToId);
  return result;
}

SyncDataLocal::SyncDataLocal(const SyncData& sync_data) : SyncData(sync_data) {
  DCHECK(sync_data.IsLocal());
}

SyncDataLocal::~SyncDataLocal() {}

const std::string& SyncDataLocal::GetTag() const {
  return immutable_entity_.Get().client_defined_unique_tag();
}

SyncDataRemote::SyncDataRemote(const SyncData& sync_data)
    : SyncData(sync_data) {
  DCHECK(!sync_data.IsLocal());
}

SyncDataRemote::~SyncDataRemote() {}

const base::Time& SyncDataRemote::GetModifiedTime() const {
  return remote_modification_time_;
}

int64 SyncDataRemote::GetId() const {
  return id_;
}

void SyncDataRemote::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const AttachmentService::GetOrDownloadCallback& callback) {
  attachment_service_.GetOrDownloadAttachments(attachment_ids, callback);
}

void SyncDataRemote::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const AttachmentService::DropCallback& callback) {
  attachment_service_.DropAttachments(attachment_ids, callback);
}

}  // namespace syncer
