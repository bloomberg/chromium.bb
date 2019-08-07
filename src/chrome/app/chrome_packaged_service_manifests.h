// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_PACKAGED_SERVICE_MANIFESTS_H_
#define CHROME_APP_CHROME_PACKAGED_SERVICE_MANIFESTS_H_

#include <vector>

#include "services/service_manager/public/cpp/manifest.h"

// Returns manifests for all shared (i.e. cross-profile) services packaged by
// Chrome but not packaged by Content. This includes both in- and out-of-process
// services.
const std::vector<service_manager::Manifest>&
GetChromePackagedServiceManifests();

#endif  // CHROME_APP_CHROME_PACKAGED_SERVICE_MANIFESTS_H_
