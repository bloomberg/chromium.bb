// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_

#include "base/string16.h"
#include "webkit/plugins/webkit_plugins_export.h"

class Version;

namespace webkit {
namespace npapi {

// Parse a version string as used by a plug-in. This method is more lenient
// in accepting weird version strings than Version::GetFromString()
WEBKIT_PLUGINS_EXPORT void CreateVersionFromString(
    const base::string16& version_string,
    Version* parsed_version);

// Returns true iff NPAPI plugins are supported on the current platform.
WEBKIT_PLUGINS_EXPORT bool NPAPIPluginsSupported();
}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_
