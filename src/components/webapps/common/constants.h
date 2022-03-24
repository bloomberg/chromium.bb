// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_COMMON_CONSTANTS_H_
#define COMPONENTS_WEBAPPS_COMMON_CONSTANTS_H_

#include <stddef.h>

namespace webapps {

// The largest reasonable length we'd assume for a meta tag attribute.
extern const size_t kMaxMetaTagAttributeLength;

// Pref key that refers to list of all apps that have been migrated to web apps.
// TODO(https://crbug.com/1266574):
// Remove this after preinstalled apps are migrated.
extern const char kWebAppsMigratedPreinstalledApps[];

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_COMMON_CONSTANTS_H_
