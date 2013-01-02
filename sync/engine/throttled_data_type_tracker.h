// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_THROTTLED_DATA_TYPE_TRACKER_H_
#define SYNC_ENGINE_THROTTLED_DATA_TYPE_TRACKER_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace base {
class TimeTicks;
}

namespace syncer {

class AllStatus;

class SYNC_EXPORT_PRIVATE ThrottledDataTypeTracker {
 public:
  // The given allstatus argument will be kept up to date on this object's list
  // of throttled types.  The argument may be NULL in tests.
  explicit ThrottledDataTypeTracker(AllStatus* allstatus);
  ~ThrottledDataTypeTracker();

  // Throttles a set of data types until the specified time is reached.
  void SetUnthrottleTime(ModelTypeSet types, const base::TimeTicks& time);

  // Given an input of the current time (usually from time::Now()), removes from
  // the set of throttled types any types whose throttling period has expired.
  void PruneUnthrottledTypes(const base::TimeTicks& time);

  // Returns the set of types which are currently throttled.
  ModelTypeSet GetThrottledTypes() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ThrottledDataTypeTrackerTest,
                           AddUnthrottleTimeTest);
  FRIEND_TEST_ALL_PREFIXES(ThrottledDataTypeTrackerTest,
                           GetCurrentlyThrottledTypesTest);

  typedef std::map<ModelType, base::TimeTicks> UnthrottleTimes;

  // This is a map from throttled data types to the time at which they can be
  // unthrottled.
  UnthrottleTimes unthrottle_times_;

  AllStatus* allstatus_;

  DISALLOW_COPY_AND_ASSIGN(ThrottledDataTypeTracker);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_THROTTLED_DATA_TYPE_TRACKER_H_
