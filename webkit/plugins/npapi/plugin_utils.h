// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_

#include "base/strings/string16.h"

namespace base {
class Version;
}

namespace webkit {
namespace npapi {

// Parse a version string as used by a plug-in. This method is more lenient
// in accepting weird version strings than base::Version::GetFromString()
void CreateVersionFromString(
    const base::string16& version_string,
    base::Version* parsed_version);

// Returns true iff NPAPI plugins are supported on the current platform.
bool NPAPIPluginsSupported();

// Tells the plugin thread to terminate the process forcefully instead of
// exiting cleanly.
void SetForcefullyTerminatePluginProcess(bool value);

// Returns true if the plugin thread should terminate the process forcefully
// instead of exiting cleanly.
bool ShouldForcefullyTerminatePluginProcess();


}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_
