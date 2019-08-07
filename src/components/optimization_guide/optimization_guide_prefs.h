// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PREFS_H_

#include "base/macros.h"

class PrefRegistrySimple;

namespace optimization_guide {
namespace prefs {

extern const char kHintLoadedCounts[];
extern const char kHintsFetcherTopHostBlacklist[];

// Registers the optimization guide's prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PREFS_H_
