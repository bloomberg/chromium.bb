// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server_entity.h"

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

using std::string;
using std::vector;

using syncer::ModelType;

// The separator used when formatting IDs.
//
// We chose the underscore character because it doesn't conflict with the
// special characters used by base/base64.h's encoding, which is also used in
// the construction of some IDs.
const char kIdSeparator[] = "_";

namespace fake_server {

FakeServerEntity::~FakeServerEntity() { }

const std::string& FakeServerEntity::GetId() const {
  return id_;
}

ModelType FakeServerEntity::GetModelType() const {
  return model_type_;
}

int64 FakeServerEntity::GetVersion() const {
  return version_;
}

void FakeServerEntity::SetVersion(int64 version) {
  version_ = version;
}

const std::string& FakeServerEntity::GetName() const {
  return name_;
}

// static
string FakeServerEntity::CreateId(const ModelType& model_type,
                                  const string& inner_id) {
  int field_number = GetSpecificsFieldNumberFromModelType(model_type);
  return base::StringPrintf("%d%s%s",
                            field_number,
                            kIdSeparator,
                            inner_id.c_str());
}

// static
std::string FakeServerEntity::GetTopLevelId(const ModelType& model_type) {
  return FakeServerEntity::CreateId(
      model_type,
      syncer::ModelTypeToRootTag(model_type));
}

// static
ModelType FakeServerEntity::GetModelTypeFromId(const string& id) {
  vector<string> tokens;
  size_t token_count = Tokenize(id, kIdSeparator, &tokens);

  int field_number;
  if (token_count != 2 || !base::StringToInt(tokens[0], &field_number)) {
    return syncer::UNSPECIFIED;
  }

  return syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
}

FakeServerEntity::FakeServerEntity(const string& id,
                                   const ModelType& model_type,
                                   int64 version,
                                   const string& name)
      : model_type_(model_type),
        id_(id),
        version_(version),
        name_(name) {}

void FakeServerEntity::SerializeBaseProtoFields(
    sync_pb::SyncEntity* sync_entity) {
  // FakeServerEntity fields
  sync_entity->set_id_string(id_);
  sync_entity->set_version(version_);
  sync_entity->set_name(name_);

  // Data via accessors
  sync_entity->set_deleted(IsDeleted());
  sync_entity->set_folder(IsFolder());
}

}  // namespace fake_server
