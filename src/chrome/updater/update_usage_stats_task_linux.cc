// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <string>
#include <vector>

namespace updater {

bool UpdateUsageStatsTask::UsageStatsAllowed(
    const std::vector<std::string>& app_ids) const {
  // TODO(crbug.com/1296311): Implement.
  return false;
}

}  // namespace updater
