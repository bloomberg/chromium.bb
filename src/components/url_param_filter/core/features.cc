// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/core/features.h"

#include "base/feature_list.h"

namespace url_param_filter::features {

const base::Feature kIncognitoParamFilterEnabled{
    "IncognitoParamFilterEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace url_param_filter::features
