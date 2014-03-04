// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/get_updates_delegate.h"

#include "sync/engine/directory_update_handler.h"
#include "sync/engine/get_updates_processor.h"

namespace syncer {

namespace {

void NonPassiveApplyUpdates(
    sessions::StatusController* status_controller,
    UpdateHandlerMap* update_handler_map) {
  for (UpdateHandlerMap::iterator it = update_handler_map->begin();
       it != update_handler_map->end(); ++it) {
    it->second->ApplyUpdates(status_controller);
  }
}

void PassiveApplyUpdates(
    sessions::StatusController* status_controller,
    UpdateHandlerMap* update_handler_map) {
  for (UpdateHandlerMap::iterator it = update_handler_map->begin();
       it != update_handler_map->end(); ++it) {
    it->second->PassiveApplyUpdates(status_controller);
  }
}

}  // namespace

GetUpdatesDelegate::GetUpdatesDelegate() {}

GetUpdatesDelegate::~GetUpdatesDelegate() {}

NormalGetUpdatesDelegate::NormalGetUpdatesDelegate(
    const sessions::NudgeTracker& nudge_tracker)
  : nudge_tracker_(nudge_tracker) {}

NormalGetUpdatesDelegate::~NormalGetUpdatesDelegate() {}

// This function assumes the progress markers have already been populated.
void NormalGetUpdatesDelegate::HelpPopulateGuMessage(
    sync_pb::GetUpdatesMessage* get_updates) const {
  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      nudge_tracker_.GetLegacySource());

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::GU_TRIGGER);
  get_updates->set_is_retry(nudge_tracker_.IsRetryRequired());

  // Special case: A GU performed for no other reason than retry will have its
  // origin set to RETRY.
  if (nudge_tracker_.GetLegacySource() == sync_pb::GetUpdatesCallerInfo::RETRY)
    get_updates->set_get_updates_origin(sync_pb::SyncEnums::RETRY);

  // Fill in the notification hints.
  for (int i = 0; i < get_updates->from_progress_marker_size(); ++i) {
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->mutable_from_progress_marker(i);
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        progress_marker->data_type_id());

    DCHECK(!nudge_tracker_.IsTypeThrottled(type))
        << "Throttled types should have been removed from the request_types.";

    nudge_tracker_.SetLegacyNotificationHint(type, progress_marker);
    nudge_tracker_.FillProtoMessage(
        type,
        progress_marker->mutable_get_update_triggers());
  }
}

void NormalGetUpdatesDelegate::ApplyUpdates(
    sessions::StatusController* status_controller,
    UpdateHandlerMap* update_handler_map) const {
  NonPassiveApplyUpdates(status_controller, update_handler_map);
}

ConfigureGetUpdatesDelegate::ConfigureGetUpdatesDelegate(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) : source_(source) {}

ConfigureGetUpdatesDelegate::~ConfigureGetUpdatesDelegate() {}

void ConfigureGetUpdatesDelegate::HelpPopulateGuMessage(
    sync_pb::GetUpdatesMessage* get_updates) const {
  get_updates->mutable_caller_info()->set_source(source_);
  get_updates->set_get_updates_origin(ConvertConfigureSourceToOrigin(source_));
}

void ConfigureGetUpdatesDelegate::ApplyUpdates(
    sessions::StatusController* status_controller,
    UpdateHandlerMap* update_handler_map) const {
  PassiveApplyUpdates(status_controller, update_handler_map);
}

sync_pb::SyncEnums::GetUpdatesOrigin
ConfigureGetUpdatesDelegate::ConvertConfigureSourceToOrigin(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  switch (source) {
    // Configurations:
    case sync_pb::GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE:
      return sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE;
    case sync_pb::GetUpdatesCallerInfo::MIGRATION:
      return sync_pb::SyncEnums::MIGRATION;
    case sync_pb::GetUpdatesCallerInfo::RECONFIGURATION:
      return sync_pb::SyncEnums::RECONFIGURATION;
    case sync_pb::GetUpdatesCallerInfo::NEW_CLIENT:
      return sync_pb::SyncEnums::NEW_CLIENT;
    default:
      NOTREACHED();
      return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
  }
}

PollGetUpdatesDelegate::PollGetUpdatesDelegate() {}

PollGetUpdatesDelegate::~PollGetUpdatesDelegate() {}

void PollGetUpdatesDelegate::HelpPopulateGuMessage(
    sync_pb::GetUpdatesMessage* get_updates) const {
  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      sync_pb::GetUpdatesCallerInfo::PERIODIC);

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::PERIODIC);
}

void PollGetUpdatesDelegate::ApplyUpdates(
    sessions::StatusController* status_controller,
    UpdateHandlerMap* update_handler_map) const {
  NonPassiveApplyUpdates(status_controller, update_handler_map);
}

}  // namespace syncer
