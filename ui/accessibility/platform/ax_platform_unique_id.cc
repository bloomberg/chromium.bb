// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_unique_id.h"

namespace ui {

int32_t GetNextAXPlatformNodeUniqueId() {
  static int32_t next_unique_id = 1;
  int32_t unique_id = next_unique_id;
  if (next_unique_id == INT32_MAX)
    next_unique_id = 1;
  else
    next_unique_id++;

  return unique_id;
}

}  // namespace ui
