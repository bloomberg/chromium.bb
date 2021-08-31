// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/gcm_features.h"

namespace gcm {
namespace features {

const base::Feature kGCMDeleteIncomingMessagesWithoutTTL(
    {"GCMDeleteIncomingMessagesWithoutTTL", base::FEATURE_DISABLED_BY_DEFAULT});

}  // namespace features
}  // namespace gcm
