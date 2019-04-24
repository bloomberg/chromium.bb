// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_LOCAL_STATE_MANIFEST_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_LOCAL_STATE_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace prefs {

const service_manager::Manifest& GetLocalStateManifest();

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_LOCAL_STATE_MANIFEST_H_
