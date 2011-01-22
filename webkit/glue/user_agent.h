// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_USER_AGENT_H_
#define WEBKIT_GLUE_USER_AGENT_H_

#include <string>

#include "base/basictypes.h"

namespace webkit_glue {

// Construct the User-Agent header, filling in |result|.
// The other parameters are workarounds for broken websites:
// - If mimic_windows is true, produce a fake Windows Chrome string.
void BuildUserAgent(bool mimic_windows, std::string* result);

// Builds a User-agent compatible string that describes the OS and CPU type.
std::string BuildOSCpuInfo();

// Returns the WebKit version, in the form "major.minor (branch@revision)".
std::string GetWebKitVersion();

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_USER_AGENT_H_

