// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_STATISTICS_WEIGHTED_MOVING_LINEAR_REGRESSION_H_
#define CHROMECAST_BASE_STATISTICS_WEIGHTED_MOVING_LINEAR_REGRESSION_H_

#include <stdint.h>
#include <queue>

#include "base/macros.h"
#include "chromecast/base/statistics/weighted_mean.h"

namespace chromecast {

// Performs linear regression over a set of weighted (x, y) samples.
// Calculates a weighted moving average over a set of weighted (x, y) points.
// The points do not need to be evenly distributed on the X axis, but the
// X coordinate is assumed to be generally increasing.
//
// Whenever a new sample is added using AddSample(), old samples whose
// x coordinates are farther than |max_x_range_| from the new sample's
// x coordinate will be removed from the regression. Note that |max_x_range_|
// must be non-negative.
class WeightedMovingLinearRegression {
 public:
  struct Sample {
    int64_t x;
    int64_t y;
    double weight;
  };

  explicit WeightedMovingLinearRegression(int64_t max_x_range);
  ~WeightedMovingLinearRegression();

  // Returns the current number of samples that are in the regression.
  size_t num_samples() const { return samples_.size(); }

  // Adds a weighted (x, y) sample to the set. Note that |weight|
  // should be positive.
  void AddSample(int64_t x, int64_t y, double weight);

  // Gets a y value estimate from the linear regression: y = a*x + b, where
  // a and b are the slope and intercept estimates from the regression. The
  // standard error of the resulting y estimate is also provided.
  // Returns false if the y value cannot be estimated, in which case y and
  // |error| are not modified. Returns true otherwise.
  bool EstimateY(int64_t x, int64_t* y, double* error) const;

  // Gets the current estimated slope and slope error from the linear
  // regression. Returns false if the slope cannot be estimated, in which
  // case |slope| and |error| are not modified. Returns true otherwise.
  bool EstimateSlope(double* slope, double* error) const;

  // Dumps samples currently in the linear regression.
  void DumpSamples() const;

  // Returns a const reference to the samples currently captured. Very
  // useful for debugging.
  const std::deque<Sample>& samples() { return samples_; }

 private:
  // Adds (x, y) to the set if |weight| is positive; removes (x, y) from the
  // set if |weight| is negative.
  void UpdateSet(int64_t x, int64_t y, double weight);

  const int64_t max_x_range_;
  WeightedMean x_mean_;
  WeightedMean y_mean_;
  double covariance_;
  std::deque<Sample> samples_;

  double slope_;
  double slope_variance_;
  double intercept_variance_;

  bool has_estimate_;

  DISALLOW_COPY_AND_ASSIGN(WeightedMovingLinearRegression);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_STATISTICS_WEIGHTED_MOVING_LINEAR_REGRESSION_H_
