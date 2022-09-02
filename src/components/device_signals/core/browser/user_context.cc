// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_context.h"

namespace device_signals {

bool operator==(const UserContext& l, const UserContext& r) {
  return l.user_id == r.user_id;
}

}  // namespace device_signals
