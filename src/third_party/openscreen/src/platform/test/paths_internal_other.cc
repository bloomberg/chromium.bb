// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/paths_internal.h"
#include "util/osp_logging.h"

namespace openscreen {

// NOTE: This is only for linking purposes in Chromium builds.
std::string GetExePath() {
  OSP_NOTREACHED();
  return {};
}

}  // namespace openscreen
