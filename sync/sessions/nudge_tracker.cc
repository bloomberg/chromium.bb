// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/nudge_tracker.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {
namespace sessions {

NudgeTracker::NudgeTracker() { }

NudgeTracker::~NudgeTracker() { }

void NudgeTracker::CoalesceSources(const SyncSourceInfo& source) {
  CoalesceStates(source.types, &source_info_.types);
  source_info_.updates_source = source.updates_source;
}

bool NudgeTracker::IsEmpty() {
  return source_info_.types.empty();
}

void NudgeTracker::Reset() {
  source_info_ = SyncSourceInfo();
}

// TODO(rlarocque): This function often reports incorrect results.  However, it
// is compatible with the "classic" behaviour.  We would need to make the nudge
// tracker stop overwriting its own information (ie. fix crbug.com/231693)
// before we could even try to report correct results.  The main issue is that
// an notifications and local modifications nudges may overlap with each other
// in sych a way that we lose track of which types were or were not locally
// modified.
ModelTypeSet NudgeTracker::GetLocallyModifiedTypes() const {
  ModelTypeSet locally_modified;

  if (source_info_.updates_source != sync_pb::GetUpdatesCallerInfo::LOCAL) {
    return locally_modified;
  }

  for (ModelTypeInvalidationMap::const_iterator i = source_info_.types.begin();
       i != source_info().types.end(); ++i) {
    locally_modified.Put(i->first);
  }
  return locally_modified;
}

}  // namespace sessions
}  // namespace syncer
