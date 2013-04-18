// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to track the outstanding work required to bring the client back into
// sync with the server.
#ifndef SYNC_SESSIONS_NUDGE_TRACKER_H_
#define SYNC_SESSIONS_NUDGE_TRACKER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/sessions/sync_source_info.h"

namespace syncer {
namespace sessions {

struct SyncSourceInfo;

class SYNC_EXPORT_PRIVATE NudgeTracker {
 public:
  NudgeTracker();
  ~NudgeTracker();

  // Merges in the information from another nudge.
  void CoalesceSources(const SyncSourceInfo& source);

  // Returns true if there are no unserviced nudges.
  bool IsEmpty();

  // Clear all unserviced nudges.
  void Reset();

  // Returns the coalesced source info.
  const SyncSourceInfo& source_info() const {
    return source_info_;
  }

  // Returns the set of locally modified types, according to our tracked source
  // infos.  The result is often wrong; see implementation comment for details.
  ModelTypeSet GetLocallyModifiedTypes() const;

 private:
  // Merged source info for the nudge(s).
  SyncSourceInfo source_info_;

  DISALLOW_COPY_AND_ASSIGN(NudgeTracker);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_NUDGE_TRACKER_H_
