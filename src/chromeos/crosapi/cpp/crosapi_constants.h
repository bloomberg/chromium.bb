// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_CROSAPI_CONSTANTS_H_
#define CHROMEOS_CROSAPI_CPP_CROSAPI_CONSTANTS_H_

#include "base/component_export.h"

namespace crosapi {

COMPONENT_EXPORT(CROSAPI) extern const char kLacrosAppIdPrefix[];

COMPONENT_EXPORT(CROSAPI) extern const char kHomeChronosUserPath[];

COMPONENT_EXPORT(CROSAPI) extern const char kMyFilesPath[];

COMPONENT_EXPORT(CROSAPI) extern const char kDefaultDownloadsPath[];

COMPONENT_EXPORT(CROSAPI) extern const char kLacrosUserDataPath[];

COMPONENT_EXPORT(CROSAPI) extern const char kChromeOSReleaseTrack[];

COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelCanary[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelDev[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelBeta[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelStable[];

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_CROSAPI_CONSTANTS_H_
