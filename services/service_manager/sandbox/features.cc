// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/features.h"

#include "build/build_config.h"

namespace service_manager {
namespace features {

#if defined(OS_WIN)
// Emergency "off switch" for new Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
const base::Feature kWinSboxDisableExtensionPoints{
    "WinSboxDisableExtensionPoint", base::FEATURE_ENABLED_BY_DEFAULT};

// Emergency "off switch" for new Windows sandbox security mitigation,
// sandbox::MITIGATION_FORCE_MS_SIGNED_BINS.
const base::Feature kWinSboxForceMsSigned{"WinSboxForceMsSigned",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

}  // namespace features
}  // namespace service_manager
