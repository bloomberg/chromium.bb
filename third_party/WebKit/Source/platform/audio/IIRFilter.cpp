// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/audio/IIRFilter.h"

#include <complex>
#include "platform/wtf/MathExtras.h"

namespace blink {

// The length of the memory buffers for the IIR filter.  This MUST be a power of
// two and must be greater than the possible length of the filter coefficients.
const int kBufferLength = 32;
static_assert(kBufferLength >= IIRFilter::kMaxOrder + 1,
              "Internal IIR buffer length must be greater than maximum IIR "
              "Filter order.");

IIRFilter::IIRFilter(const AudioDoubleArray* feedforward,
                     const AudioDoubleArray* feedback)
    : buffer_index_(0), feedback_(feedback), feedforward_(feedforward) {
  // These are guaranteed to be zero-initialized.
  x_buffer_.Allocate(kBufferLength);
  y_buffer_.Allocate(kBufferLength);
}

IIRFilter::~IIRFilter() {}

void IIRFilter::Reset() {
  x_buffer_.Zero();
  y_buffer_.Zero();
}

static std::complex<double> EvaluatePolynomial(const double* coef,
                                               std::complex<double> z,
                                               int order) {
  // Use Horner's method to evaluate the polynomial P(z) = sum(coef[k]*z^k, k,
  // 0, order);
  std::complex<double> result = 0;

  for (int k = order; k >= 0; --k)
    result = result * z + std::complex<double>(coef[k]);

  return result;
}

void IIRFilter::Process(const float* source_p,
                        float* dest_p,
                        size_t frames_to_process) {
  // Compute
  //
  //   y[n] = sum(b[k] * x[n - k], k = 0, M) - sum(a[k] * y[n - k], k = 1, N)
  //
  // where b[k] are the feedforward coefficients and a[k] are the feedback
  // coefficients of the filter.

  // This is a Direct Form I implementation of an IIR Filter.  Should we
  // consider doing a different implementation such as Transposed Direct Form
  // II?
  const double* feedback = feedback_->Data();
  const double* feedforward = feedforward_->Data();

  DCHECK(feedback);
  DCHECK(feedforward);

  // Sanity check to see if the feedback coefficients have been scaled
  // appropriately. It must be EXACTLY 1!
  DCHECK_EQ(feedback[0], 1);

  int feedback_length = feedback_->size();
  int feedforward_length = feedforward_->size();
  int min_length = std::min(feedback_length, feedforward_length);

  double* x_buffer = x_buffer_.Data();
  double* y_buffer = y_buffer_.Data();

  for (size_t n = 0; n < frames_to_process; ++n) {
    // To help minimize roundoff, we compute using double's, even though the
    // filter coefficients only have single precision values.
    double yn = feedforward[0] * source_p[n];

    // Run both the feedforward and feedback terms together, when possible.
    for (int k = 1; k < min_length; ++k) {
      int n = (buffer_index_ - k) & (kBufferLength - 1);
      yn += feedforward[k] * x_buffer[n];
      yn -= feedback[k] * y_buffer[n];
    }

    // Handle any remaining feedforward or feedback terms.
    for (int k = min_length; k < feedforward_length; ++k)
      yn +=
          feedforward[k] * x_buffer[(buffer_index_ - k) & (kBufferLength - 1)];

    for (int k = min_length; k < feedback_length; ++k)
      yn -= feedback[k] * y_buffer[(buffer_index_ - k) & (kBufferLength - 1)];

    // Save the current input and output values in the memory buffers for the
    // next output.
    x_buffer_[buffer_index_] = source_p[n];
    y_buffer_[buffer_index_] = yn;

    buffer_index_ = (buffer_index_ + 1) & (kBufferLength - 1);

    dest_p[n] = yn;
  }
}

void IIRFilter::GetFrequencyResponse(int n_frequencies,
                                     const float* frequency,
                                     float* mag_response,
                                     float* phase_response) {
  // Evaluate the z-transform of the filter at the given normalized frequencies
  // from 0 to 1. (One corresponds to the Nyquist frequency.)
  //
  // The z-tranform of the filter is
  //
  // H(z) = sum(b[k]*z^(-k), k, 0, M) / sum(a[k]*z^(-k), k, 0, N);
  //
  // The desired frequency response is H(exp(j*omega)), where omega is in [0,
  // 1).
  //
  // Let P(x) = sum(c[k]*x^k, k, 0, P) be a polynomial of order P.  Then each of
  // the sums in H(z) is equivalent to evaluating a polynomial at the point
  // 1/z.

  for (int k = 0; k < n_frequencies; ++k) {
    // zRecip = 1/z = exp(-j*frequency)
    double omega = -piDouble * frequency[k];
    std::complex<double> z_recip = std::complex<double>(cos(omega), sin(omega));

    std::complex<double> numerator = EvaluatePolynomial(
        feedforward_->Data(), z_recip, feedforward_->size() - 1);
    std::complex<double> denominator =
        EvaluatePolynomial(feedback_->Data(), z_recip, feedback_->size() - 1);
    std::complex<double> response = numerator / denominator;
    mag_response[k] = static_cast<float>(abs(response));
    phase_response[k] =
        static_cast<float>(atan2(imag(response), real(response)));
  }
}

}  // namespace blink
