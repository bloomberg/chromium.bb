// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Returns the string than can be used to record histograms for the optimization
// target. If adding a histogram to use the string or adding an optimization
// target, update the OptimizationGuide.OptimizationTargets histogram suffixes
// in histograms.xml.
std::string GetStringNameForOptimizationTarget(
    proto::OptimizationTarget optimization_target);

// Returns the file path represented by the given string, handling platform
// differences in the conversion. nullopt is only returned iff the passed string
// is empty.
absl::optional<base::FilePath> StringToFilePath(const std::string& str_path);

// Returns the base file name to use for storing all prediction models.
base::FilePath GetBaseFileNameForModels();

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_
