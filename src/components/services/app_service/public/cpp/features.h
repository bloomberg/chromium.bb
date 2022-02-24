// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FEATURES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace apps {

COMPONENT_EXPORT(APP_UPDATE)
extern const base::Feature kAppServiceOnAppTypeInitializedWithoutMojom;
extern const base::Feature kAppServiceOnAppUpdateWithoutMojom;

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FEATURES_H_
