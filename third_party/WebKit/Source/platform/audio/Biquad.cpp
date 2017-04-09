/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/audio/Biquad.h"

#include "platform/audio/AudioUtilities.h"
#include "platform/audio/DenormalDisabler.h"
#include "platform/wtf/MathExtras.h"

#include <algorithm>
#include <complex>
#include <stdio.h>
#if OS(MACOSX)
#include <Accelerate/Accelerate.h>
#endif

namespace blink {

#if OS(MACOSX)
const int kBufferSize = 1024;
#endif

Biquad::Biquad() : has_sample_accurate_values_(false) {
#if OS(MACOSX)
  // Allocate two samples more for filter history
  input_buffer_.Allocate(kBufferSize + 2);
  output_buffer_.Allocate(kBufferSize + 2);
#endif

  // Allocate enough space for the a-rate filter coefficients to handle a
  // rendering quantum of 128 frames.
  b0_.Allocate(AudioUtilities::kRenderQuantumFrames);
  b1_.Allocate(AudioUtilities::kRenderQuantumFrames);
  b2_.Allocate(AudioUtilities::kRenderQuantumFrames);
  a1_.Allocate(AudioUtilities::kRenderQuantumFrames);
  a2_.Allocate(AudioUtilities::kRenderQuantumFrames);

  // Initialize as pass-thru (straight-wire, no filter effect)
  SetNormalizedCoefficients(0, 1, 0, 0, 1, 0, 0);

  Reset();  // clear filter memory
}

Biquad::~Biquad() {}

void Biquad::Process(const float* source_p,
                     float* dest_p,
                     size_t frames_to_process) {
  // WARNING: sourceP and destP may be pointing to the same area of memory!
  // Be sure to read from sourceP before writing to destP!
  if (HasSampleAccurateValues()) {
    int n = frames_to_process;

    // Create local copies of member variables
    double x1 = x1_;
    double x2 = x2_;
    double y1 = y1_;
    double y2 = y2_;

    const double* b0 = b0_.Data();
    const double* b1 = b1_.Data();
    const double* b2 = b2_.Data();
    const double* a1 = a1_.Data();
    const double* a2 = a2_.Data();

    for (int k = 0; k < n; ++k) {
      // FIXME: this can be optimized by pipelining the multiply adds...
      float x = *source_p++;
      float y = b0[k] * x + b1[k] * x1 + b2[k] * x2 - a1[k] * y1 - a2[k] * y2;

      *dest_p++ = y;

      // Update state variables
      x2 = x1;
      x1 = x;
      y2 = y1;
      y1 = y;
    }

    // Local variables back to member. Flush denormals here so we
    // don't slow down the inner loop above.
    x1_ = DenormalDisabler::FlushDenormalFloatToZero(x1);
    x2_ = DenormalDisabler::FlushDenormalFloatToZero(x2);
    y1_ = DenormalDisabler::FlushDenormalFloatToZero(y1);
    y2_ = DenormalDisabler::FlushDenormalFloatToZero(y2);

    // There is an assumption here that once we have sample accurate values we
    // can never go back to not having sample accurate values.  This is
    // currently true in the way AudioParamTimline is implemented: once an
    // event is inserted, sample accurate processing is always enabled.
    //
    // If so, then we never have to update the state variables for the MACOSX
    // path.  The structure of the state variable in these cases aren't well
    // documented so it's not clear how to update them anyway.
  } else {
#if OS(MACOSX)
    double* input_p = input_buffer_.Data();
    double* output_p = output_buffer_.Data();

    // Set up filter state.  This is needed in case we're switching from
    // filtering with variable coefficients (i.e., with automations) to
    // fixed coefficients (without automations).
    input_p[0] = x2_;
    input_p[1] = x1_;
    output_p[0] = y2_;
    output_p[1] = y1_;

    // Use vecLib if available
    ProcessFast(source_p, dest_p, frames_to_process);

    // Copy the last inputs and outputs to the filter memory variables.
    // This is needed because the next rendering quantum might be an
    // automation which needs the history to continue correctly.  Because
    // sourceP and destP can be the same block of memory, we can't read from
    // sourceP to get the last inputs.  Fortunately, processFast has put the
    // last inputs in input[0] and input[1].
    x1_ = input_p[1];
    x2_ = input_p[0];
    y1_ = dest_p[frames_to_process - 1];
    y2_ = dest_p[frames_to_process - 2];

#else
    int n = frames_to_process;

    // Create local copies of member variables
    double x1 = x1_;
    double x2 = x2_;
    double y1 = y1_;
    double y2 = y2_;

    double b0 = b0_[0];
    double b1 = b1_[0];
    double b2 = b2_[0];
    double a1 = a1_[0];
    double a2 = a2_[0];

    while (n--) {
      // FIXME: this can be optimized by pipelining the multiply adds...
      float x = *source_p++;
      float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

      *dest_p++ = y;

      // Update state variables
      x2 = x1;
      x1 = x;
      y2 = y1;
      y1 = y;
    }

    // Local variables back to member. Flush denormals here so we
    // don't slow down the inner loop above.
    x1_ = DenormalDisabler::FlushDenormalFloatToZero(x1);
    x2_ = DenormalDisabler::FlushDenormalFloatToZero(x2);
    y1_ = DenormalDisabler::FlushDenormalFloatToZero(y1);
    y2_ = DenormalDisabler::FlushDenormalFloatToZero(y2);
#endif
  }
}

#if OS(MACOSX)

// Here we have optimized version using Accelerate.framework

void Biquad::ProcessFast(const float* source_p,
                         float* dest_p,
                         size_t frames_to_process) {
  double filter_coefficients[5];
  filter_coefficients[0] = b0_[0];
  filter_coefficients[1] = b1_[0];
  filter_coefficients[2] = b2_[0];
  filter_coefficients[3] = a1_[0];
  filter_coefficients[4] = a2_[0];

  double* input_p = input_buffer_.Data();
  double* output_p = output_buffer_.Data();

  double* input2p = input_p + 2;
  double* output2p = output_p + 2;

  // Break up processing into smaller slices (kBufferSize) if necessary.

  int n = frames_to_process;

  while (n > 0) {
    int frames_this_time = n < kBufferSize ? n : kBufferSize;

    // Copy input to input buffer
    for (int i = 0; i < frames_this_time; ++i)
      input2p[i] = *source_p++;

    ProcessSliceFast(input_p, output_p, filter_coefficients, frames_this_time);

    // Copy output buffer to output (converts float -> double).
    for (int i = 0; i < frames_this_time; ++i)
      *dest_p++ = static_cast<float>(output2p[i]);

    n -= frames_this_time;
  }
}

void Biquad::ProcessSliceFast(double* source_p,
                              double* dest_p,
                              double* coefficients_p,
                              size_t frames_to_process) {
  // Use double-precision for filter stability
  vDSP_deq22D(source_p, 1, coefficients_p, dest_p, 1, frames_to_process);

  // Save history.  Note that sourceP and destP reference m_inputBuffer and
  // m_outputBuffer respectively.  These buffers are allocated (in the
  // constructor) with space for two extra samples so it's OK to access array
  // values two beyond framesToProcess.
  source_p[0] = source_p[frames_to_process - 2 + 2];
  source_p[1] = source_p[frames_to_process - 1 + 2];
  dest_p[0] = dest_p[frames_to_process - 2 + 2];
  dest_p[1] = dest_p[frames_to_process - 1 + 2];
}

#endif  // OS(MACOSX)

void Biquad::Reset() {
#if OS(MACOSX)
  // Two extra samples for filter history
  double* input_p = input_buffer_.Data();
  input_p[0] = 0;
  input_p[1] = 0;

  double* output_p = output_buffer_.Data();
  output_p[0] = 0;
  output_p[1] = 0;

#endif
  x1_ = x2_ = y1_ = y2_ = 0;
}

void Biquad::SetLowpassParams(int index, double cutoff, double resonance) {
  // Limit cutoff to 0 to 1.
  cutoff = clampTo(cutoff, 0.0, 1.0);

  if (cutoff == 1) {
    // When cutoff is 1, the z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  } else if (cutoff > 0) {
    // Compute biquad coefficients for lowpass filter

    resonance = pow(10, resonance / 20);
    double theta = piDouble * cutoff;
    double alpha = sin(theta) / (2 * resonance);
    double cosw = cos(theta);
    double beta = (1 - cosw) / 2;

    double b0 = beta;
    double b1 = 2 * beta;
    double b2 = beta;

    double a0 = 1 + alpha;
    double a1 = -2 * cosw;
    double a2 = 1 - alpha;

    SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
  } else {
    // When cutoff is zero, nothing gets through the filter, so set
    // coefficients up correctly.
    SetNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetHighpassParams(int index, double cutoff, double resonance) {
  // Limit cutoff to 0 to 1.
  cutoff = clampTo(cutoff, 0.0, 1.0);

  if (cutoff == 1) {
    // The z-transform is 0.
    SetNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
  } else if (cutoff > 0) {
    // Compute biquad coefficients for highpass filter

    resonance = pow(10, resonance / 20);
    double theta = piDouble * cutoff;
    double alpha = sin(theta) / (2 * resonance);
    double cosw = cos(theta);
    double beta = (1 + cosw) / 2;

    double b0 = beta;
    double b1 = -2 * beta;
    double b2 = beta;

    double a0 = 1 + alpha;
    double a1 = -2 * cosw;
    double a2 = 1 - alpha;

    SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
  } else {
    // When cutoff is zero, we need to be careful because the above
    // gives a quadratic divided by the same quadratic, with poles
    // and zeros on the unit circle in the same place. When cutoff
    // is zero, the z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetNormalizedCoefficients(int index,
                                       double b0,
                                       double b1,
                                       double b2,
                                       double a0,
                                       double a1,
                                       double a2) {
  double a0_inverse = 1 / a0;

  b0_[index] = b0 * a0_inverse;
  b1_[index] = b1 * a0_inverse;
  b2_[index] = b2 * a0_inverse;
  a1_[index] = a1 * a0_inverse;
  a2_[index] = a2 * a0_inverse;
}

void Biquad::SetLowShelfParams(int index, double frequency, double db_gain) {
  // Clip frequencies to between 0 and 1, inclusive.
  frequency = clampTo(frequency, 0.0, 1.0);

  double a = pow(10.0, db_gain / 40);

  if (frequency == 1) {
    // The z-transform is a constant gain.
    SetNormalizedCoefficients(index, a * a, 0, 0, 1, 0, 0);
  } else if (frequency > 0) {
    double w0 = piDouble * frequency;
    double s = 1;  // filter slope (1 is max value)
    double alpha = 0.5 * sin(w0) * sqrt((a + 1 / a) * (1 / s - 1) + 2);
    double k = cos(w0);
    double k2 = 2 * sqrt(a) * alpha;
    double a_plus_one = a + 1;
    double a_minus_one = a - 1;

    double b0 = a * (a_plus_one - a_minus_one * k + k2);
    double b1 = 2 * a * (a_minus_one - a_plus_one * k);
    double b2 = a * (a_plus_one - a_minus_one * k - k2);
    double a0 = a_plus_one + a_minus_one * k + k2;
    double a1 = -2 * (a_minus_one + a_plus_one * k);
    double a2 = a_plus_one + a_minus_one * k - k2;

    SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
  } else {
    // When frequency is 0, the z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetHighShelfParams(int index, double frequency, double db_gain) {
  // Clip frequencies to between 0 and 1, inclusive.
  frequency = clampTo(frequency, 0.0, 1.0);

  double a = pow(10.0, db_gain / 40);

  if (frequency == 1) {
    // The z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  } else if (frequency > 0) {
    double w0 = piDouble * frequency;
    double s = 1;  // filter slope (1 is max value)
    double alpha = 0.5 * sin(w0) * sqrt((a + 1 / a) * (1 / s - 1) + 2);
    double k = cos(w0);
    double k2 = 2 * sqrt(a) * alpha;
    double a_plus_one = a + 1;
    double a_minus_one = a - 1;

    double b0 = a * (a_plus_one + a_minus_one * k + k2);
    double b1 = -2 * a * (a_minus_one + a_plus_one * k);
    double b2 = a * (a_plus_one + a_minus_one * k - k2);
    double a0 = a_plus_one - a_minus_one * k + k2;
    double a1 = 2 * (a_minus_one - a_plus_one * k);
    double a2 = a_plus_one - a_minus_one * k - k2;

    SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
  } else {
    // When frequency = 0, the filter is just a gain, A^2.
    SetNormalizedCoefficients(index, a * a, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetPeakingParams(int index,
                              double frequency,
                              double q,
                              double db_gain) {
  // Clip frequencies to between 0 and 1, inclusive.
  frequency = clampTo(frequency, 0.0, 1.0);

  // Don't let Q go negative, which causes an unstable filter.
  q = std::max(0.0, q);

  double a = pow(10.0, db_gain / 40);

  if (frequency > 0 && frequency < 1) {
    if (q > 0) {
      double w0 = piDouble * frequency;
      double alpha = sin(w0) / (2 * q);
      double k = cos(w0);

      double b0 = 1 + alpha * a;
      double b1 = -2 * k;
      double b2 = 1 - alpha * a;
      double a0 = 1 + alpha / a;
      double a1 = -2 * k;
      double a2 = 1 - alpha / a;

      SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
      // When Q = 0, the above formulas have problems. If we look at
      // the z-transform, we can see that the limit as Q->0 is A^2, so
      // set the filter that way.
      SetNormalizedCoefficients(index, a * a, 0, 0, 1, 0, 0);
    }
  } else {
    // When frequency is 0 or 1, the z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetAllpassParams(int index, double frequency, double q) {
  // Clip frequencies to between 0 and 1, inclusive.
  frequency = clampTo(frequency, 0.0, 1.0);

  // Don't let Q go negative, which causes an unstable filter.
  q = std::max(0.0, q);

  if (frequency > 0 && frequency < 1) {
    if (q > 0) {
      double w0 = piDouble * frequency;
      double alpha = sin(w0) / (2 * q);
      double k = cos(w0);

      double b0 = 1 - alpha;
      double b1 = -2 * k;
      double b2 = 1 + alpha;
      double a0 = 1 + alpha;
      double a1 = -2 * k;
      double a2 = 1 - alpha;

      SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
      // When Q = 0, the above formulas have problems. If we look at
      // the z-transform, we can see that the limit as Q->0 is -1, so
      // set the filter that way.
      SetNormalizedCoefficients(index, -1, 0, 0, 1, 0, 0);
    }
  } else {
    // When frequency is 0 or 1, the z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetNotchParams(int index, double frequency, double q) {
  // Clip frequencies to between 0 and 1, inclusive.
  frequency = clampTo(frequency, 0.0, 1.0);

  // Don't let Q go negative, which causes an unstable filter.
  q = std::max(0.0, q);

  if (frequency > 0 && frequency < 1) {
    if (q > 0) {
      double w0 = piDouble * frequency;
      double alpha = sin(w0) / (2 * q);
      double k = cos(w0);

      double b0 = 1;
      double b1 = -2 * k;
      double b2 = 1;
      double a0 = 1 + alpha;
      double a1 = -2 * k;
      double a2 = 1 - alpha;

      SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
      // When Q = 0, the above formulas have problems. If we look at
      // the z-transform, we can see that the limit as Q->0 is 0, so
      // set the filter that way.
      SetNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
    }
  } else {
    // When frequency is 0 or 1, the z-transform is 1.
    SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
  }
}

void Biquad::SetBandpassParams(int index, double frequency, double q) {
  // No negative frequencies allowed.
  frequency = std::max(0.0, frequency);

  // Don't let Q go negative, which causes an unstable filter.
  q = std::max(0.0, q);

  if (frequency > 0 && frequency < 1) {
    double w0 = piDouble * frequency;
    if (q > 0) {
      double alpha = sin(w0) / (2 * q);
      double k = cos(w0);

      double b0 = alpha;
      double b1 = 0;
      double b2 = -alpha;
      double a0 = 1 + alpha;
      double a1 = -2 * k;
      double a2 = 1 - alpha;

      SetNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
      // When Q = 0, the above formulas have problems. If we look at
      // the z-transform, we can see that the limit as Q->0 is 1, so
      // set the filter that way.
      SetNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    }
  } else {
    // When the cutoff is zero, the z-transform approaches 0, if Q
    // > 0. When both Q and cutoff are zero, the z-transform is
    // pretty much undefined. What should we do in this case?
    // For now, just make the filter 0. When the cutoff is 1, the
    // z-transform also approaches 0.
    SetNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
  }
}

void Biquad::GetFrequencyResponse(int n_frequencies,
                                  const float* frequency,
                                  float* mag_response,
                                  float* phase_response) {
  // Evaluate the Z-transform of the filter at given normalized
  // frequency from 0 to 1.  (1 corresponds to the Nyquist
  // frequency.)
  //
  // The z-transform of the filter is
  //
  // H(z) = (b0 + b1*z^(-1) + b2*z^(-2))/(1 + a1*z^(-1) + a2*z^(-2))
  //
  // Evaluate as
  //
  // b0 + (b1 + b2*z1)*z1
  // --------------------
  // 1 + (a1 + a2*z1)*z1
  //
  // with z1 = 1/z and z = exp(j*pi*frequency). Hence z1 = exp(-j*pi*frequency)

  // Make local copies of the coefficients as a micro-optimization.
  double b0 = b0_[0];
  double b1 = b1_[0];
  double b2 = b2_[0];
  double a1 = a1_[0];
  double a2 = a2_[0];

  for (int k = 0; k < n_frequencies; ++k) {
    double omega = -piDouble * frequency[k];
    std::complex<double> z = std::complex<double>(cos(omega), sin(omega));
    std::complex<double> numerator = b0 + (b1 + b2 * z) * z;
    std::complex<double> denominator =
        std::complex<double>(1, 0) + (a1 + a2 * z) * z;
    std::complex<double> response = numerator / denominator;
    mag_response[k] = static_cast<float>(abs(response));
    phase_response[k] =
        static_cast<float>(atan2(imag(response), real(response)));
  }
}

}  // namespace blink
