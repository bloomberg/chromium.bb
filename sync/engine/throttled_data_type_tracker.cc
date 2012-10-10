// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/throttled_data_type_tracker.h"

#include "base/time.h"
#include "sync/engine/all_status.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

ThrottledDataTypeTracker::ThrottledDataTypeTracker(AllStatus *allstatus)
    : allstatus_(allstatus) {
  if (allstatus_) {
    allstatus_->SetThrottledTypes(ModelTypeSet());
  }
}

ThrottledDataTypeTracker::~ThrottledDataTypeTracker() { }

void ThrottledDataTypeTracker::SetUnthrottleTime(ModelTypeSet types,
                                                 const base::TimeTicks& time) {
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    unthrottle_times_[it.Get()] = time;
  }

  DVLOG(2)
      << "Throttling types: " << ModelTypeSetToString(types)
      << ", throttled list is now: "
      << ModelTypeSetToString(GetThrottledTypes());
  if (allstatus_) {
    allstatus_->SetThrottledTypes(GetThrottledTypes());
  }
}

void ThrottledDataTypeTracker::PruneUnthrottledTypes(
    const base::TimeTicks& time) {
  bool modified = false;

  UnthrottleTimes::iterator it = unthrottle_times_.begin();
  while (it != unthrottle_times_.end()) {
    if (it->second <= time) {
      // Delete and increment the iterator.
      UnthrottleTimes::iterator iterator_to_delete = it;
      ++it;
      unthrottle_times_.erase(iterator_to_delete);
      modified = true;
    } else {
      // Just increment the iterator.
      ++it;
    }
  }

  DVLOG_IF(2, modified)
      << "Remaining throttled types: "
      << ModelTypeSetToString(GetThrottledTypes());
  if (modified && allstatus_) {
    allstatus_->SetThrottledTypes(GetThrottledTypes());
  }
}

ModelTypeSet ThrottledDataTypeTracker::GetThrottledTypes() const {
  ModelTypeSet types;
  for (UnthrottleTimes::const_iterator it = unthrottle_times_.begin();
       it != unthrottle_times_.end(); ++it) {
    types.Put(it->first);
  }
  return types;
}

}  // namespace syncer
