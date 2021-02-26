// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace crosapi {

// The prefix for a Wayland app id for a Lacros browser window. The full ID is
// suffixed with a serialized unguessable token unique to each window. The
// trailing "." is intentional.
const char kLacrosAppIdPrefix[] = "org.chromium.lacros.";

// Path to the ash-side primary user profile directory, which is a hard link to
// a directory in the encrypted user data partition.
const char kHomeChronosUserPath[] = "/home/chronos/user";

// The "MyFiles" directory for the ash-side primary user.
const char kMyFilesPath[] = "/home/chronos/user/MyFiles";

// The "Downloads" directory for the ash-side primary user. Note that the user
// can choose to download files to a different directory, see DownloadPrefs.
const char kDefaultDownloadsPath[] = "/home/chronos/user/MyFiles/Downloads";

// The default user-data-directory for Lacros.
// NOTE: This is security sensitive. The directory must be inside the encrypted
// user data partition.
const char kLacrosUserDataPath[] = "/home/chronos/user/lacros";

// Release channel key in /etc/lsb-release.
const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";

// Release channel values in /etc/lsb-release.
const char kReleaseChannelCanary[] = "canary-channel";
const char kReleaseChannelDev[] = "dev-channel";
const char kReleaseChannelBeta[] = "beta-channel";
const char kReleaseChannelStable[] = "stable-channel";

}  // namespace crosapi
