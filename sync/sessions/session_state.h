// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The 'sessions' namespace comprises all the pieces of state that are
// combined to form a SyncSession instance. In that way, it can be thought of
// as an extension of the SyncSession type itself. Session scoping gives
// context to things like "conflict progress", "update progress", etc, and the
// separation this file provides allows clients to only include the parts they
// need rather than the entire session stack.

#ifndef SYNC_SESSIONS_SESSION_STATE_H_
#define SYNC_SESSIONS_SESSION_STATE_H_

#include <set>

#include "sync/syncable/syncable_id.h"

namespace syncer {
namespace sessions {

// Grouping of all state that applies to a single ModelSafeGroup.
struct PerModelSafeGroupState {
  explicit PerModelSafeGroupState();
  ~PerModelSafeGroupState();

  std::set<syncable::Id> simple_conflict_ids;
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_SESSION_STATE_H_
