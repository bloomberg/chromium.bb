// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_

#include "build/build_config.h"

namespace security_interstitials {

// Provides utilities for security interstitials on //content-based platforms.

#if !defined(OS_CHROMEOS) && !defined(OS_FUCHSIA)
// Launches date and time settings as appropriate based on the platform (not
// supported on ChromeOS, where taking this action requires embedder-level
// machinery, or Fuchsia, which simply doesn't require this functionality).
void LaunchDateAndTimeSettings();
#endif

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_
