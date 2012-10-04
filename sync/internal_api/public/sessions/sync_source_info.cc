// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/sync_source_info.h"

#include "base/values.h"
#include "sync/protocol/proto_enum_conversions.h"

namespace syncer {
namespace sessions {

SyncSourceInfo::SyncSourceInfo()
    : updates_source(sync_pb::GetUpdatesCallerInfo::UNKNOWN) {}

SyncSourceInfo::SyncSourceInfo(const ModelTypeInvalidationMap& t)
    : updates_source(sync_pb::GetUpdatesCallerInfo::UNKNOWN), types(t) {}

SyncSourceInfo::SyncSourceInfo(
    const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& u,
    const ModelTypeInvalidationMap& t)
    : updates_source(u), types(t) {}

SyncSourceInfo::~SyncSourceInfo() {}

DictionaryValue* SyncSourceInfo::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  value->SetString("updatesSource",
                   GetUpdatesSourceString(updates_source));
  value->Set("types", ModelTypeInvalidationMapToValue(types));
  return value;
}

}  // namespace sessions
}  // namespace syncer
