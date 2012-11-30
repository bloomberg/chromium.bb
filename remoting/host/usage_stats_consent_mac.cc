// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <string>

#include "base/logging.h"

namespace remoting {

bool GetUsageStatsConsent(bool* allowed, bool* set_by_policy) {
  *set_by_policy = false;

  // For now, leave Breakpad disabled.
  // TODO(lambroslambrou): Enable it for users who have given consent.
  *allowed = false;
  return true;
}

bool IsUsageStatsAllowed() {
  bool allowed;
  bool set_by_policy;
  return GetUsageStatsConsent(&allowed, &set_by_policy) && allowed;
}

bool SetUsageStatsConsent(bool allowed) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace remoting
