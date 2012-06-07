// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_ERROR_COUNTERS_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_ERROR_COUNTERS_H_
#pragma once

#include "base/basictypes.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync_protocol_error.h"

namespace browser_sync {
namespace sessions {

// Counters for various errors that can occur repeatedly during a sync session.
// TODO(lipalani) : Rename this structure to Error.
struct ErrorCounters {
  ErrorCounters();

  // Any protocol errors that we received during this sync session.
  SyncProtocolError sync_protocol_error;

  // Records the most recent results of PostCommit and GetUpdates commands.
  SyncerError last_download_updates_result;
  SyncerError last_post_commit_result;
  SyncerError last_process_commit_response_result;
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_ERROR_COUNTERS_H_
