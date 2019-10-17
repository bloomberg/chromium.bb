// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_MODEL_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_MODEL_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

struct EVENTS_OZONE_EVDEV_EXPORT NeuralStylusPalmDetectionFilterModelConfig {
  // Explicit constructor to make chromium style happy.
  NeuralStylusPalmDetectionFilterModelConfig();
  NeuralStylusPalmDetectionFilterModelConfig(
      const NeuralStylusPalmDetectionFilterModelConfig& other);

  // Number of nearest neighbors to use in vector construction.
  uint32_t nearest_neighbor_count = 0;

  // Number of biggest nearby neighbors to use in vector construction.
  uint32_t biggest_near_neighbor_count = 0;

  // Maximum distance of neighbor centroid, in millimeters.
  float max_neighbor_distance_in_mm = 0.0f;

  base::TimeDelta max_dead_neighbor_time =
      base::TimeDelta::FromMilliseconds(0.0);

  // Minimum count of samples in a stroke for neural comparison.
  uint32_t min_sample_count = 0;

  // Maximum sample count.
  uint32_t max_sample_count = 0;

  uint32_t max_sequence_start_count_for_inference = 0;

  bool include_sequence_count_in_strokes = false;

  // If this number is positive, short strokes with a touch major greater than
  // or equal to this should be marked as a palm. If 0 or less, has no effect.
  float heuristic_palm_touch_limit = 0.0f;

  // If this number is positive, short strokes with any touch having an area
  // greater than or equal to this should be marked as a palm. If <= 0, has no
  // effect
  float heuristic_palm_area_limit = 0.0f;
};

// An abstract model utilized by NueralStylusPalmDetectionFilter.
class EVENTS_OZONE_EVDEV_EXPORT NeuralStylusPalmDetectionFilterModel {
 public:
  virtual ~NeuralStylusPalmDetectionFilterModel() {}

  // Actually execute inference on floating point input. If the length of
  // features is not correct, return Nan. The return value is assumed to be the
  // input of a sigmoid. i.e. any value greater than 0 implies a positive
  // result.
  virtual float Inference(const std::vector<float>& features) const = 0;

  virtual NeuralStylusPalmDetectionFilterModelConfig& config() const = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_MODEL_H_
