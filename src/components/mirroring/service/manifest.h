// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MANIFEST_H_
#define COMPONENTS_MIRRORING_SERVICE_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace mirroring {

const service_manager::Manifest& GetManifest();

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MANIFEST_H_
