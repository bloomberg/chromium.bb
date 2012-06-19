// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONSTANTS_MAC_H_
#define REMOTING_HOST_CONSTANTS_MAC_H_

namespace remoting {

// The name of the Remoting Host service that is registered with launchd.
#define kServiceName "org.chromium.chromoting"

// Use separate named notifications for success and failure because sandboxed
// components can't include a dictionary when sending distributed notifications.
// The preferences panel is not yet sandboxed, but err on the side of caution.
#define kUpdateSucceededNotificationName kServiceName ".update_succeeded"
#define kUpdateFailedNotificationName kServiceName ".update_failed"

#define kHostConfigDir "/Library/PrivilegedHelperTools/"

// This helper tool is executed as root to enable/disable/configure the host
// service.
// It is also used (as non-root) to provide version information for the
// installed host components.
extern const char kHostHelperTool[];

}  // namespace remoting

#endif  // REMOTING_HOST_CONSTANTS_MAC_H_
