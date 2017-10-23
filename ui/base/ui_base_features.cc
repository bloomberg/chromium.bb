// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

namespace features {

#if defined(OS_WIN)
// Enables stylus appearing as touch when in contact with digitizer.
const base::Feature kDirectManipulationStylus = {
    "DirectManipulationStylus", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

// Applies the material design mode passed via --top-chrome-md to elements
// throughout Chrome (not just top Chrome).
const base::Feature kSecondaryUiMd = {"SecondaryUiMd",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
