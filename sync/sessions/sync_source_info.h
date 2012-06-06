// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SESSIONS_SYNC_SOURCE_INFO_H_
#define SYNC_SESSIONS_SYNC_SOURCE_INFO_H_
#pragma once

#include "base/basictypes.h"
#include "sync/engine/syncproto.h"
#include "sync/syncable/model_type.h"
#include "sync/syncable/model_type_payload_map.h"

namespace base {
class DictionaryValue;
}

namespace browser_sync {
namespace sessions {

// A container for the source of a sync session. This includes the update
// source, the datatypes triggering the sync session, and possible session
// specific payloads which should be sent to the server.
struct SyncSourceInfo {
  SyncSourceInfo();
  explicit SyncSourceInfo(const syncable::ModelTypePayloadMap& t);
  SyncSourceInfo(
      const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& u,
      const syncable::ModelTypePayloadMap& t);
  ~SyncSourceInfo();

  // Caller takes ownership of the returned dictionary.
  base::DictionaryValue* ToValue() const;

  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source;
  syncable::ModelTypePayloadMap types;
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // SYNC_SESSIONS_SYNC_SOURCE_INFO_H_
