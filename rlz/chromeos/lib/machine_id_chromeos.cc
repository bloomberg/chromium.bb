// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/machine_id.h"

namespace rlz_lib {

bool GetRawMachineId(base::string16* data, int* more_data) {
  // Machine IDs are not tracked for ChromeOS.
  return false;
}

}  // namespace rlz_lib
