// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RETURN_CALLBACK_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RETURN_CALLBACK_H_

#include "base/callback.h"
#include "components/viz/common/resources/returned_resource.h"

namespace viz {

using ReturnCallback =
    base::RepeatingCallback<void(const std::vector<ReturnedResource>&)>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RETURN_CALLBACK_H_
