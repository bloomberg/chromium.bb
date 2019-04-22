// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/features.h"

namespace printing {
namespace features {

#if defined(OS_ANDROID)
const base::Feature kUsePdfCompositorServiceForPrint{
    "UsePdfCompositorServiceForPrint", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kUsePdfCompositorServiceForPrint{
    "UsePdfCompositorServiceForPrint", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_MACOSX)
const base::Feature kEnableCustomMacPaperSizes{
    "EnableCustomMacPaperSizes", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace printing
