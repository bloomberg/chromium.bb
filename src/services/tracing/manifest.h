// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_MANIFEST_H_
#define SERVICES_TRACING_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace tracing {

const service_manager::Manifest& GetManifest();

}  // namespace tracing

#endif  // SERVICES_TRACING_MANIFEST_H_
