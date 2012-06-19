// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_USER_AGENT_H_
#define WEBKIT_GLUE_USER_AGENT_H_

#include <string>

#include "base/basictypes.h"

namespace webkit_glue {

// Builds a User-agent compatible string that describes the OS and CPU type.
std::string BuildOSCpuInfo();

// Returns the WebKit version, in the form "major.minor (branch@revision)".
std::string GetWebKitVersion();

// The following 2 functions return the major and minor webkit versions.
int GetWebKitMajorVersion();
int GetWebKitMinorVersion();

#if defined(OS_ANDROID)
// Sets the OS component of the user agent (e.g. "4.0.4; Galaxy Nexus
// BUILD/IMM76K")
// TODO(yfriedman): Remove this ASAP (http://crbug.com/131312)
void SetUserAgentOSInfo(const std::string& os_info);
#endif

// Helper function to generate a full user agent string from a short
// product name.
std::string BuildUserAgentFromProduct(const std::string& product);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_USER_AGENT_H_
