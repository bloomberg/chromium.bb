// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/time.h"

#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"

namespace csync {

int64 TimeToProtoTime(const base::Time& t) {
  return (t - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time ProtoTimeToTime(int64 proto_t) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(proto_t);
}

std::string GetTimeDebugString(const base::Time& t) {
  return UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(t));
}

}  // namespace csync
