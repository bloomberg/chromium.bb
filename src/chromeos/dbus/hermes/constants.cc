// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/constants.h"

namespace chromeos {
namespace hermes_constants {

// 2min * 60s/min * 1000ms/s.
int kHermesNetworkOperationTimeoutMs = 2 * 60 * 1000;

}  // namespace hermes_constants
}  // namespace chromeos
