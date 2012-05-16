// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The AllStatus object watches various sync engine components and aggregates
// the status of all of them into one place.

#ifndef SYNC_INTERNAL_API_ALL_STATUS_H_
#define SYNC_INTERNAL_API_ALL_STATUS_H_
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/sync_engine_event.h"
#include "sync/internal_api/sync_manager.h"
#include "sync/syncable/model_type.h"

namespace browser_sync {

class ScopedStatusLock;
struct ServerConnectionEvent;

// TODO(rlarocque):
// Most of this data ends up on the about:sync page.  But the page is only
// 'pinged' to update itself at the end of a sync cycle.  A user could refresh
// manually, but unless their timing is excellent it's unlikely that a user will
// see any state in mid-sync cycle.  We have no plans to change this.
//
// What we do intend to do is improve the UI so that changes following a sync
// cycle are more visible.  Without such a change, the status summary for a
// healthy syncer will constantly display as "READY" and never provide any
// indication of a sync cycle being performed.  See crbug.com/108100.

class AllStatus : public SyncEngineEventListener {
  friend class ScopedStatusLock;
 public:
  AllStatus();
  virtual ~AllStatus();

  virtual void OnSyncEngineEvent(const SyncEngineEvent& event) OVERRIDE;

  sync_api::SyncManager::Status status() const;

  void SetNotificationsEnabled(bool notifications_enabled);

  void IncrementNotifiableCommits();

  void IncrementNotificationsReceived();

  void SetEncryptedTypes(syncable::ModelTypeSet types);
  void SetCryptographerReady(bool ready);
  void SetCryptoHasPendingKeys(bool has_pending_keys);

  void SetUniqueId(const std::string& guid);

 protected:
  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  sync_api::SyncManager::Status CalcSyncing(const SyncEngineEvent& event) const;
  sync_api::SyncManager::Status CreateBlankStatus() const;

  sync_api::SyncManager::Status status_;

  mutable base::Lock mutex_;  // Protects all data members.
  DISALLOW_COPY_AND_ASSIGN(AllStatus);
};

class ScopedStatusLock {
 public:
  explicit ScopedStatusLock(AllStatus* allstatus);
  ~ScopedStatusLock();
 protected:
  AllStatus* allstatus_;
};

}  // namespace browser_sync

#endif  // SYNC_INTERNAL_API_ALL_STATUS_H_
