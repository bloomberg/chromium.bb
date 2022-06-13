// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_utils.h"

#include "chrome/updater/constants.h"

namespace updater {

bool ShouldUninstall(const std::vector<std::string>& app_ids,
                     int server_starts) {
  return app_ids.size() <= 1 && server_starts > kMaxServerStartsBeforeFirstReg;
}

}  // namespace updater
