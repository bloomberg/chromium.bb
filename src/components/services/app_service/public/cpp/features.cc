// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/features.h"

namespace apps {

const base::Feature kAppServiceOnAppTypeInitializedWithoutMojom{
    "AppServiceOnAppTypeInitializedWithoutMojom",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAppServiceOnAppUpdateWithoutMojom{
    "AppServiceOnAppUpdateWithoutMojom", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace apps
