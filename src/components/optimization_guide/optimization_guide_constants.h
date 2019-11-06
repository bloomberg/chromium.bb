// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_CONSTANTS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace optimization_guide {

// The name of the file that stores the unindexed hints.
extern const base::FilePath::CharType kUnindexedHintsFileName[];

extern const char kRulesetFormatVersionString[];

// The remote Optimization Guide Service production server to fetch hints from.
extern const char kOptimizationGuideServiceDefaultURL[];

// The local histogram used to record that the component hints are stored in
// the cache and are ready for use.
extern const char kLoadedHintLocalHistogramString[];

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_CONSTANTS_H_
