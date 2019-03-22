// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_

#include <memory>

namespace optimization_guide {

struct HintsComponentInfo;
namespace proto {
class Configuration;
}  // namespace proto

// The UMA histogram used to record the result of processing the hints
// component.
extern const char kProcessHintsComponentResultHistogramString[];

// Enumerates the possible outcomes of processing the hints component. Used in
// UMA histograms, so the order of enumerators should not be changed.
//
// Keep in sync with OptimizationGuideProcessHintsResult in
// tools/metrics/histograms/enums.xml.
enum class ProcessHintsComponentResult {
  SUCCESS,
  FAILED_INVALID_PARAMETERS,
  FAILED_READING_FILE,
  FAILED_INVALID_CONFIGURATION,

  // Insert new values before this line.
  MAX,
};

// Processes the specified hints component, records the result in a UMA
// histogram, and, if successful, returns the component's Configuration
// protobuf. If unsuccessful, returns a nullptr.
std::unique_ptr<proto::Configuration> ProcessHintsComponent(
    const HintsComponentInfo& info);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_
