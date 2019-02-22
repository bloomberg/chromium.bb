// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"

#include <cmath>

#include "base/logging.h"
#include "base/macros.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {
constexpr double kTol = 1e-10;

bool IsIncreasing(const std::vector<double>& data, bool is_strict) {
  DCHECK_GT(data.size(), 1u);
  for (size_t i = 1; i < data.size(); ++i) {
    if (data[i] < data[i - 1] || (data[i] <= data[i - 1] && is_strict))
      return false;
  }
  return true;
}

// Computes the tangents at every control point as the average of the secants,
// while ensuring monotonicity is preserved.
std::vector<double> ComputeTangents(const std::vector<double>& xs,
                                    const std::vector<double>& ys,
                                    size_t num_points) {
  DCHECK_GT(num_points, 1u);
  DCHECK_EQ(num_points, ys.size());
  DCHECK(IsIncreasing(xs, true /* is_strict */));
  DCHECK(IsIncreasing(ys, false /* is_strict */));

  // Calculate the slopes of the secant lines between successive points.
  std::vector<double> ds;
  std::vector<double> ms;
  for (size_t i = 0; i < num_points - 1; ++i) {
    const double slope = (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]);
    DCHECK_GE(slope, 0);
    ds.push_back(slope);
  }

  // Initialize the tangents at every point as the average of the secants, and
  // use one-sided differences for endpoints.
  ms.push_back(ds[0]);
  for (size_t i = 1; i < num_points - 1; ++i) {
    ms.push_back(0.5 * (ds[i - 1] + ds[i]));
  }
  ms.push_back(ds[num_points - 2]);

  // Refine tangents to ensure spline monotonicity.
  for (size_t i = 0; i < num_points - 1; ++i) {
    if (ds[i] < kTol) {
      // Successive points are equal, spline needs to be flat.
      ms[i] = 0;
      ms[i + 1] = 0;
    } else {
      const double a = ms[i] / ds[i];
      const double b = ms[i + 1] / ds[i];
      DCHECK_GE(a, 0.0);
      DCHECK_GE(b, 0.0);

      const double r = std::hypot(a, b);
      if (r > 3.0) {
        const double t = 3.0 / r;
        ms[i] *= t;
        ms[i + 1] *= t;
      }
    }
  }

  return ms;
}

}  // namespace

MonotoneCubicSpline::MonotoneCubicSpline(const std::vector<double>& xs,
                                         const std::vector<double>& ys)
    : xs_(xs),
      ys_(ys),
      num_points_(xs.size()),
      ms_(ComputeTangents(xs, ys, num_points_)) {}

MonotoneCubicSpline::MonotoneCubicSpline(const MonotoneCubicSpline& spline) =
    default;

MonotoneCubicSpline::~MonotoneCubicSpline() = default;

double MonotoneCubicSpline::Interpolate(double x) const {
  DCHECK_GT(num_points_, 1u);

  if (x <= xs_[0])
    return ys_[0];

  if (x >= xs_.back())
    return ys_.back();

  // Get |x_lower| and |x_upper| so that |x_lower| <= |x| <= |x_upper|.
  // Size of |xs_| is small, so linear search for upper & lower
  // bounds will be ok.
  size_t i = 1;
  while (i < num_points_) {
    const double curr = xs_[i];
    if (curr == x) {
      // Return exact value if |x| is a control point.
      return ys_[i];
    }
    if (curr > x) {
      break;
    }
    ++i;
  }

  DCHECK_LT(i, num_points_);
  const double x_upper = xs_[i];
  const double x_lower = xs_[i - 1];
  DCHECK_GE(x, x_lower);
  DCHECK_LE(x, x_upper);

  const double h = x_upper - x_lower;
  const double t = (x - x_lower) / h;

  return (ys_[i - 1] * (2 * t + 1) + h * ms_[i - 1] * t) * (t - 1) * (t - 1) +
         (ys_[i] * (-2 * t + 3) + h * ms_[i] * (t - 1)) * t * t;
}

std::vector<double> MonotoneCubicSpline::GetControlPointsX() const {
  return xs_;
}

std::vector<double> MonotoneCubicSpline::GetControlPointsY() const {
  return ys_;
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
