// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/syncer_status.h"

#include "base/values.h"

namespace csync {
namespace sessions {

SyncerStatus::SyncerStatus()
    : invalid_store(false),
      num_successful_commits(0),
      num_successful_bookmark_commits(0),
      num_updates_downloaded_total(0),
      num_tombstone_updates_downloaded_total(0),
      num_reflected_updates_downloaded_total(0),
      num_local_overwrites(0),
      num_server_overwrites(0) {
}

SyncerStatus::~SyncerStatus() {
}

DictionaryValue* SyncerStatus::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  value->SetBoolean("invalidStore", invalid_store);
  value->SetInteger("numSuccessfulCommits", num_successful_commits);
  value->SetInteger("numSuccessfulBookmarkCommits",
                num_successful_bookmark_commits);
  value->SetInteger("numUpdatesDownloadedTotal",
                num_updates_downloaded_total);
  value->SetInteger("numTombstoneUpdatesDownloadedTotal",
                num_tombstone_updates_downloaded_total);
  value->SetInteger("numReflectedUpdatesDownloadedTotal",
                num_reflected_updates_downloaded_total);
  value->SetInteger("numLocalOverwrites", num_local_overwrites);
  value->SetInteger("numServerOverwrites", num_server_overwrites);
  return value;
}

}  // namespace sessions
}  // namespace csync
