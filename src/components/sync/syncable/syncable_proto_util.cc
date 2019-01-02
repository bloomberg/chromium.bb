// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/syncable_proto_util.h"

#include "components/sync/protocol/sync.pb.h"

namespace syncer {

syncable::Id SyncableIdFromProto(const std::string& proto_string) {
  return syncable::Id::CreateFromServerId(proto_string);
}

std::string SyncableIdToProto(const syncable::Id& syncable_id) {
  return syncable_id.GetServerId();
}

bool IsFolder(const sync_pb::SyncEntity& entity) {
  return entity.folder();
}

bool IsRoot(const sync_pb::SyncEntity& entity) {
  return SyncableIdFromProto(entity.id_string()).IsRoot();
}

}  // namespace syncer
