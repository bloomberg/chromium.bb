// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_GET_UPDATES_DELEGATE_H_
#define SYNC_ENGINE_GET_UPDATES_DELEGATE_H_

#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/model_type_registry.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/sessions/status_controller.h"

namespace syncer {

class GetUpdatesProcessor;

// Interface for GetUpdates functionality that dependends on the requested
// GetUpdate type (normal, configuration, poll).  The GetUpdatesProcessor is
// given an appropriate GetUpdatesDelegate to handle type specific functionality
// on construction.
class SYNC_EXPORT_PRIVATE GetUpdatesDelegate {
 public:
  GetUpdatesDelegate();
  virtual ~GetUpdatesDelegate() = 0;

  // Populates GetUpdate message fields that depende on GetUpdates request type.
  virtual void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const = 0;

  // Applies pending updates to non-control types.
  virtual void ApplyUpdates(
      ModelTypeSet gu_types,
      sessions::StatusController* status,
      UpdateHandlerMap* update_handler_map) const = 0;

  virtual scoped_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const = 0;
};

// Functionality specific to the normal GetUpdate request.
class SYNC_EXPORT_PRIVATE NormalGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  NormalGetUpdatesDelegate(const sessions::NudgeTracker& nudge_tracker);
  virtual ~NormalGetUpdatesDelegate();

  // Uses the member NudgeTracker to populate some fields of this GU message.
  virtual void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const OVERRIDE;

  // Applies pending updates on the appropriate data type threads.
  virtual void ApplyUpdates(
      ModelTypeSet gu_types,
      sessions::StatusController* status,
      UpdateHandlerMap* update_handler_map) const OVERRIDE;

  virtual scoped_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(NormalGetUpdatesDelegate);

  const sessions::NudgeTracker& nudge_tracker_;
};

// Functionality specific to the configure GetUpdate request.
class SYNC_EXPORT_PRIVATE ConfigureGetUpdatesDelegate
    : public GetUpdatesDelegate {
 public:
  ConfigureGetUpdatesDelegate(
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source);
  virtual ~ConfigureGetUpdatesDelegate();

  // Sets the 'source' and 'origin' fields for this request.
  virtual void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const OVERRIDE;

  // Applies updates passively (ie. on the sync thread).
  //
  // This is safe only if the ChangeProcessor is not listening to changes at
  // this time.
  virtual void ApplyUpdates(
      ModelTypeSet gu_types,
      sessions::StatusController* status,
      UpdateHandlerMap* update_handler_map) const OVERRIDE;

  virtual scoped_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigureGetUpdatesDelegate);

  static sync_pb::SyncEnums::GetUpdatesOrigin ConvertConfigureSourceToOrigin(
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source);

  const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source_;
};

// Functionality specific to the poll GetUpdate request.
class SYNC_EXPORT_PRIVATE PollGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  PollGetUpdatesDelegate();
  virtual ~PollGetUpdatesDelegate();

  // Sets the 'source' and 'origin' to indicate this is a poll request.
  virtual void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const OVERRIDE;

  // Applies updates on the appropriate data type thread.
  virtual void ApplyUpdates(
      ModelTypeSet gu_types,
      sessions::StatusController* status,
      UpdateHandlerMap* update_handler_map) const OVERRIDE;

  virtual scoped_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(PollGetUpdatesDelegate);
};

}  // namespace syncer

#endif   // SYNC_ENGINE_GET_UPDATES_DELEGATE_H_
