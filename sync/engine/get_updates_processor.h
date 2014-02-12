// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_GET_UPDATES_PROCESSOR_H
#define SYNC_ENGINE_GET_UPDATES_PROCESSOR_H

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/sessions/model_type_registry.h"

namespace sync_pb {
class GetUpdatesMessage;
class GetUpdatesResponse;
}  // namespace sync_pb

namespace syncer {

namespace sessions {
class StatusController;
}  // namespace sessions

namespace syncable {
class Directory;
}  // namespace syncable

class GetUpdatesDelegate;
class SyncDirectoryUpdateHandler;

typedef std::vector<const sync_pb::SyncEntity*> SyncEntityList;
typedef std::map<ModelType, SyncEntityList> TypeSyncEntityMap;

// This class manages the set of per-type syncer objects.
//
// It owns these types and hides the details of iterating over all of them.
// Most methods allow the caller to specify a subset of types on which the
// operation is to be applied.  It is a logic error if the supplied set of types
// contains a type which was not previously registered with the manager.
class SYNC_EXPORT_PRIVATE GetUpdatesProcessor {
 public:
  explicit GetUpdatesProcessor(UpdateHandlerMap* update_handler_map,
                               const GetUpdatesDelegate& delegate);
  ~GetUpdatesProcessor();

  // Populates a GetUpdates request message with per-type information.
  void PrepareGetUpdates(ModelTypeSet gu_types,
                         sync_pb::GetUpdatesMessage* get_updates);

  // Processes a GetUpdates responses for each type.
  bool ProcessGetUpdatesResponse(
      ModelTypeSet gu_types,
      const sync_pb::GetUpdatesResponse& gu_response,
      sessions::StatusController* status_controller);

  // Hands off control to the delegate so it can apply updates.
  void ApplyUpdates(sessions::StatusController* status_controller);

 private:
  // A map of 'update handlers', one for each enabled type.
  // This must be kept in sync with the routing info.  Our temporary solution to
  // that problem is to initialize this map in set_routing_info().
  UpdateHandlerMap* update_handler_map_;

  const GetUpdatesDelegate& delegate_;

  DISALLOW_COPY_AND_ASSIGN(GetUpdatesProcessor);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_GET_UPDATES_PROCESSOR_H_
