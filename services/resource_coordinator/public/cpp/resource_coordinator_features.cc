// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace features {

#if defined(OS_WIN)
// Empty the working set of processes in which all frames are frozen.
const base::Feature kEmptyWorkingSet{"EmptyWorkingSet",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
