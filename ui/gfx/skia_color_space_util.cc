// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_color_space_util.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"

namespace gfx {

namespace {

// Evaluate the gradient of the linear component of fn
void SkTransferFnEvalGradientLinear(const SkColorSpaceTransferFn& fn,
                                    float x,
                                    float* d_fn_d_fC_at_x,
                                    float* d_fn_d_fF_at_x) {
  *d_fn_d_fC_at_x = x;
  *d_fn_d_fF_at_x = 1.f;
}

// Solve for the parameters fC and fF, given the parameter fD.
void SkTransferFnSolveLinear(SkColorSpaceTransferFn* fn,
                             const float* x,
                             const float* t,
                             size_t n) {
  // If this has no linear segment, don't try to solve for one.
  if (fn->fD <= 0) {
    fn->fC = 1;
    fn->fF = 0;
    return;
  }

  // Let ne_lhs be the left hand side of the normal equations, and let ne_rhs
  // be the right hand side. This is a 4x4 matrix, but we will only update the
  // upper-left 2x2 sub-matrix.
  SkMatrix44 ne_lhs;
  SkVector4 ne_rhs;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      ne_lhs.set(row, col, 0);
    }
  }
  for (int row = 0; row < 4; ++row)
    ne_rhs.fData[row] = 0;

  // Add the contributions from each sample to the normal equations.
  float x_linear_max = 0;
  for (size_t i = 0; i < n; ++i) {
    // Ignore points in the nonlinear segment.
    if (x[i] >= fn->fD)
      continue;
    x_linear_max = std::max(x_linear_max, x[i]);

    // Let J be the gradient of fn with respect to parameters C and F, evaluated
    // at this point.
    float J[2];
    SkTransferFnEvalGradientLinear(*fn, x[i], &J[0], &J[1]);

    // Let r be the residual at this point.
    float r = t[i];

    // Update the normal equations left hand side with the outer product of J
    // with itself.
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 2; ++col) {
        ne_lhs.set(row, col, ne_lhs.get(row, col) + J[row] * J[col]);
      }
      // Update the normal equations right hand side the product of J with the
      // residual
      ne_rhs.fData[row] += J[row] * r;
    }
  }

  // If we only have a single x point at 0, that isn't enough to construct a
  // linear segment, so add an additional point connecting to the nonlinear
  // segment.
  if (x_linear_max == 0) {
    float J[2];
    SkTransferFnEvalGradientLinear(*fn, fn->fD, &J[0], &J[1]);
    float r = SkTransferFnEval(*fn, fn->fD);
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 2; ++col) {
        ne_lhs.set(row, col, ne_lhs.get(row, col) + J[row] * J[col]);
      }
      ne_rhs.fData[row] += J[row] * r;
    }
  }

  // Solve the normal equations.
  SkMatrix44 ne_lhs_inv;
  bool invert_result = ne_lhs.invert(&ne_lhs_inv);
  DCHECK(invert_result);
  SkVector4 solution = ne_lhs_inv * ne_rhs;

  // Update the transfer function.
  fn->fC = solution.fData[0];
  fn->fF = solution.fData[1];
}

// Evaluate the gradient of the nonlinear component of fn
void SkTransferFnEvalGradientNonlinear(const SkColorSpaceTransferFn& fn,
                                       float x,
                                       float* d_fn_d_fA_at_x,
                                       float* d_fn_d_fB_at_x,
                                       float* d_fn_d_fE_at_x,
                                       float* d_fn_d_fG_at_x) {
  float base = fn.fA * x + fn.fB;
  if (base > 0.f) {
    *d_fn_d_fA_at_x = fn.fG * x * std::pow(base, fn.fG - 1.f);
    *d_fn_d_fB_at_x = fn.fG * std::pow(base, fn.fG - 1.f);
    *d_fn_d_fE_at_x = 1.f;
    *d_fn_d_fG_at_x = std::pow(base, fn.fG) * std::log(base);
  } else {
    *d_fn_d_fA_at_x = 0.f;
    *d_fn_d_fB_at_x = 0.f;
    *d_fn_d_fE_at_x = 0.f;
    *d_fn_d_fG_at_x = 0.f;
  }
}

// Take one Gauss-Newton step updating fA, fB, fE, and fG, given fD.
bool SkTransferFnGaussNewtonStepNonlinear(SkColorSpaceTransferFn* fn,
                                          float* error_Linfty_before,
                                          const float* x,
                                          const float* t,
                                          size_t n) {
  float kEpsilon = 1.f / 1024.f;
  // Let ne_lhs be the left hand side of the normal equations, and let ne_rhs
  // be the right hand side.
  SkMatrix44 ne_lhs;
  SkVector4 ne_rhs;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      ne_lhs.set(row, col, 0);
    }
    ne_rhs.fData[row] = 0;
  }

  // Add the contributions from each sample to the normal equations.
  *error_Linfty_before = 0;
  for (size_t i = 0; i < n; ++i) {
    // Ignore points in the linear segment.
    if (x[i] < fn->fD)
      continue;
    // Let J be the gradient of fn with respect to parameters A, B, E, and G,
    // evaulated at this point.
    float J[4];
    SkTransferFnEvalGradientNonlinear(*fn, x[i], &J[0], &J[1], &J[2], &J[3]);
    // Let r be the residual at this point;
    float r = t[i] - SkTransferFnEval(*fn, x[i]);
    *error_Linfty_before += std::abs(r);
    // Update the normal equations left hand side with the outer product of J
    // with itself.
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        ne_lhs.set(row, col, ne_lhs.get(row, col) + J[row] * J[col]);
      }
      // Update the normal equations right hand side the product of J with the
      // residual
      ne_rhs.fData[row] += J[row] * r;
    }
  }

  // Note that if fG = 1, then the normal equations will be singular (because,
  // when fG = 1, because fA and fE are equivalent parameters).  To avoid
  // problems, fix fE (row/column 3) in these circumstances.
  if (std::abs(fn->fG - 1) < kEpsilon) {
    for (int row = 0; row < 4; ++row) {
      float value = (row == 2) ? 1.f : 0.f;
      ne_lhs.set(row, 2, value);
      ne_lhs.set(2, row, value);
    }
    ne_rhs.fData[2] = 0.f;
  }

  // Solve the normal equations.
  SkMatrix44 ne_lhs_inv;
  if (!ne_lhs.invert(&ne_lhs_inv))
    return false;
  SkVector4 step = ne_lhs_inv * ne_rhs;

  // Update the transfer function.
  fn->fA += step.fData[0];
  fn->fB += step.fData[1];
  fn->fE += step.fData[2];
  fn->fG += step.fData[3];
  return true;
}

// Solve for fA, fB, fE, and fG, given fD. The initial value of |fn| is the
// point from which iteration starts.
bool SkTransferFnSolveNonlinear(SkColorSpaceTransferFn* fn,
                                const float* x,
                                const float* t,
                                size_t n) {
  // Take a maximum of 16 Gauss-Newton steps.
  const size_t kNumSteps = 16;

  // The L-infinity error before each step.
  float step_error[kNumSteps] = {0};

  for (size_t step = 0; step < kNumSteps; ++step) {
    // If the normal equations are singular, we can't continue.
    if (!SkTransferFnGaussNewtonStepNonlinear(fn, &step_error[step], x, t, n))
      return false;

    // If the error is inf or nan, we are clearly not converging.
    if (std::isnan(step_error[step]) || std::isinf(step_error[step]))
      return false;

    // If our error is non-negligable and increasing then we are not in the
    // region of convergence.
    const float kNonNegligbleErrorEpsilon = 1.f / 256.f;
    const float kGrowthFactor = 1.25f;
    if (step > 2 && step_error[step] > kNonNegligbleErrorEpsilon) {
      if (step_error[step - 1] * kGrowthFactor < step_error[step] &&
          step_error[step - 2] * kGrowthFactor < step_error[step - 1]) {
        return false;
      }
    }
  }

  // We've converged to a reasonable solution. If some of the parameters are
  // extremely close to 0 or 1, set them to 0 or 1.
  const float kRoundEpsilon = 1.f / 1024.f;
  if (std::abs(fn->fA - 1.f) < kRoundEpsilon)
    fn->fA = 1.f;
  if (std::abs(fn->fB) < kRoundEpsilon)
    fn->fB = 0;
  if (std::abs(fn->fE) < kRoundEpsilon)
    fn->fE = 0;
  if (std::abs(fn->fG - 1.f) < kRoundEpsilon)
    fn->fG = 1.f;
  return true;
}

void SkApproximateTransferFnInternal(const float* x,
                                     const float* t,
                                     size_t n,
                                     SkColorSpaceTransferFn* fn,
                                     float* error,
                                     bool* nonlinear_fit_converged) {
  // First, guess at a value of fD. Assume that the nonlinear segment applies
  // to all x >= 0.1. This is generally a safe assumption (fD is usually less
  // than 0.1).
  fn->fD = 0.1f;

  // Do a nonlinear regression on the nonlinear segment. Use a number of guesses
  // for the initial value of fG, because not all values will converge.
  *nonlinear_fit_converged = false;
  {
    const size_t kNumInitialGammas = 4;
    float initial_gammas[kNumInitialGammas] = {1.f, 2.f, 3.f, 0.5f};
    for (size_t i = 0; i < kNumInitialGammas; ++i) {
      fn->fG = initial_gammas[i];
      fn->fA = 1;
      fn->fB = 0;
      fn->fC = 1;
      fn->fE = 0;
      fn->fF = 0;
      if (SkTransferFnSolveNonlinear(fn, x, t, n)) {
        *nonlinear_fit_converged = true;
        break;
      }
    }
  }

  // Now walk back fD from our initial guess to the point where our nonlinear
  // fit no longer fits (or all the way to 0 if it fits).
  if (*nonlinear_fit_converged) {
    // Find the L-infinity error of this nonlinear fit (using our old fD value).
    float max_error_in_nonlinear_fit = 0;
    for (size_t i = 0; i < n; ++i) {
      if (x[i] < fn->fD)
        continue;
      float error_at_xi = std::abs(t[i] - SkTransferFnEval(*fn, x[i]));
      max_error_in_nonlinear_fit =
          std::max(max_error_in_nonlinear_fit, error_at_xi);
    }

    // Now find the maximum x value where this nonlinear fit is no longer
    // accurate or no longer defined.
    fn->fD = 0.f;
    float max_x_where_nonlinear_does_not_fit = -1.f;
    for (size_t i = 0; i < n; ++i) {
      bool nonlinear_fits_xi = true;
      if (fn->fA * x[i] + fn->fB < 0) {
        // The nonlinear segment is undefined when fA * x + fB is less than 0.
        nonlinear_fits_xi = false;
      } else {
        // Define "no longer accurate" as "has more than 10% more error than
        // the maximum error in the fit segment".
        float error_at_xi = std::abs(t[i] - SkTransferFnEval(*fn, x[i]));
        if (error_at_xi > 1.1f * max_error_in_nonlinear_fit)
          nonlinear_fits_xi = false;
      }
      if (!nonlinear_fits_xi) {
        max_x_where_nonlinear_does_not_fit =
            std::max(max_x_where_nonlinear_does_not_fit, x[i]);
      }
    }

    // Now let fD be the highest sample of x that is above the threshold where
    // the nonlinear segment does not fit.
    fn->fD = 1.f;
    for (size_t i = 0; i < n; ++i) {
      if (x[i] > max_x_where_nonlinear_does_not_fit)
        fn->fD = std::min(fn->fD, x[i]);
    }
  } else {
    // If the nonlinear fit didn't converge, then try a linear fit on the full
    // range. Note that fD must be strictly greater than 1 because the nonlinear
    // segment starts at fD.
    const float kEpsilon = 1.f / 256.f;
    fn->fD = 1.f + kEpsilon;
  }

  // Compute the linear segment, now that we have our definitive fD.
  SkTransferFnSolveLinear(fn, x, t, n);

  // If the nonlinear fit didn't converge, re-express the linear fit in
  // canonical form as a nonlinear fit with fG as 1.
  if (!*nonlinear_fit_converged) {
    fn->fA = fn->fC;
    fn->fB = fn->fF;
    fn->fC = 1;
    fn->fD = 0;
    fn->fE = 0;
    fn->fF = 0;
    fn->fG = 1;
  }

  // Return the L-infinity error of the total approximation.
  *error = 0.f;
  for (size_t i = 0; i < n; ++i)
    *error = std::max(*error, std::abs(t[i] - SkTransferFnEval(*fn, x[i])));
}

}  // namespace

float SkTransferFnEval(const SkColorSpaceTransferFn& fn, float x) {
  if (x < 0.f)
    return 0.f;
  if (x < fn.fD)
    return fn.fC * x + fn.fF;
  return std::pow(fn.fA * x + fn.fB, fn.fG) + fn.fE;
}

SkColorSpaceTransferFn SkTransferFnInverse(const SkColorSpaceTransferFn& fn) {
  SkColorSpaceTransferFn fn_inv = {0};
  if (fn.fA > 0 && fn.fG > 0) {
    double a_to_the_g = std::pow(fn.fA, fn.fG);
    fn_inv.fA = 1.f / a_to_the_g;
    fn_inv.fB = -fn.fE / a_to_the_g;
    fn_inv.fG = 1.f / fn.fG;
  }
  fn_inv.fD = fn.fC * fn.fD + fn.fF;
  fn_inv.fE = -fn.fB / fn.fA;
  if (fn.fC != 0) {
    fn_inv.fC = 1.f / fn.fC;
    fn_inv.fF = -fn.fF / fn.fC;
  }
  return fn_inv;
}

bool SkTransferFnsApproximatelyCancel(const SkColorSpaceTransferFn& a,
                                      const SkColorSpaceTransferFn& b) {
  const float kStep = 1.f / 8.f;
  const float kEpsilon = 2.5f / 256.f;
  for (float x = 0; x <= 1.f; x += kStep) {
    float a_of_x = SkTransferFnEval(a, x);
    float b_of_a_of_x = SkTransferFnEval(b, a_of_x);
    if (std::abs(b_of_a_of_x - x) > kEpsilon)
      return false;
  }
  return true;
}

bool SkTransferFnIsApproximatelyIdentity(const SkColorSpaceTransferFn& a) {
  const float kStep = 1.f / 8.f;
  const float kEpsilon = 2.5f / 256.f;
  for (float x = 0; x <= 1.f; x += kStep) {
    float a_of_x = SkTransferFnEval(a, x);
    if (std::abs(a_of_x - x) > kEpsilon)
      return false;
  }
  return true;
}

void SkApproximateTransferFn(const float* x,
                             const float* t,
                             size_t n,
                             SkColorSpaceTransferFn* fn,
                             float* error,
                             bool* nonlinear_fit_converged) {
  SkApproximateTransferFnInternal(x, t, n, fn, error, nonlinear_fit_converged);
}

bool SkMatrixIsApproximatelyIdentity(const SkMatrix44& m) {
  const float kEpsilon = 1.f / 256.f;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float identity_value = i == j ? 1 : 0;
      float value = m.get(i, j);
      if (std::abs(identity_value - value) > kEpsilon)
        return false;
    }
  }
  return true;
}

}  // namespace gfx
