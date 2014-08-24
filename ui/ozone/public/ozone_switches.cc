// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_switches.h"

namespace switches {

// Specify ozone platform implementation to use.
const char kOzonePlatform[] = "ozone-platform";

// Specify location for image dumps.
const char kOzoneDumpFile[] = "ozone-dump-file";

// Specify if the accelerated path should use surfaceless rendering. In this
// mode there is no EGL surface.
const char kOzoneUseSurfaceless[] = "ozone-use-surfaceless";

// Enable support for a single overlay plane.
const char kOzoneTestSingleOverlaySupport[] =
    "ozone-test-single-overlay-support";

}  // namespace switches
