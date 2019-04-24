// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_

#include "base/callback_forward.h"

namespace base {
class TaskTraits;
}

// System permission state.
enum class SystemPermission {
  kNotDetermined = 0,
  kNotAllowed = 1,
  kAllowed = 2
};

// On 10.14 and above: returns if system permission is allowed or not, or not
// determined.
// On 10.13 and below: returns |SystemPermission::kAllowed|, since there are no
// system media capture permissions.
SystemPermission CheckSystemAudioCapturePermission();
SystemPermission CheckSystemVideoCapturePermission();

// On 10.14 and above: requests system permission and returns. When requesting
// permission, the OS will show a user dialog and respond asynchronously. At the
// response, |callback| is posted with |traits|.
// On 10.13 and below: posts |callback| with |traits|, since there are no system
// media capture permissions.
// Note: these functions should really never be called for pre-10.14 since one
// would normally check the permission first, and only call this if it's not
// determined.
void RequestSystemAudioCapturePermisson(base::OnceClosure callback,
                                        const base::TaskTraits& traits);
void RequestSystemVideoCapturePermisson(base::OnceClosure callback,
                                        const base::TaskTraits& traits);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
