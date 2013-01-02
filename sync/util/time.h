// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Time-related sync functions.

#ifndef SYNC_UTIL_TIME_H_
#define SYNC_UTIL_TIME_H_

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "sync/base/sync_export.h"

namespace syncer {

// Converts a time object to the format used in sync protobufs (ms
// since the Unix epoch).
SYNC_EXPORT int64 TimeToProtoTime(const base::Time& t);

// Converts a time field from sync protobufs to a time object.
SYNC_EXPORT_PRIVATE base::Time ProtoTimeToTime(int64 proto_t);

SYNC_EXPORT std::string GetTimeDebugString(const base::Time& t);

}  // namespace syncer

#endif  // SYNC_UTIL_TIME_H_
