// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WTF_USE_WEBAUDIO_PFFFT)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/pffft/src/pffft.h"

namespace blink {

// Not really clear what the largest size of FFT PFFFT supports, but the docs
// indicate it can go up to at least 1048576 (order 20).  Since we're using
// single-floats, accuracy decreases quite a bit at that size.  Plus we only
// need 32K (order 15) for WebAudio.
const unsigned kMaxFFTPow2Size = 20;

FFTFrame::FFTFrame(unsigned fft_size)
    : fft_size_(fft_size),
      log2fft_size_(static_cast<unsigned>(log2(fft_size))),
      real_data_(fft_size / 2),
      imag_data_(fft_size / 2),
      context_(nullptr),
      complex_data_(fft_size),
      pffft_work_(fft_size) {
  // We only allow power of two.
  DCHECK_EQ(1UL << log2fft_size_, fft_size_);

  context_ = ContextForSize(fft_size);
}

// Creates a blank/empty frame (interpolate() must later be called).
FFTFrame::FFTFrame() : fft_size_(0), log2fft_size_(0), context_(nullptr) {}

// Copy constructor.
FFTFrame::FFTFrame(const FFTFrame& frame)
    : fft_size_(frame.fft_size_),
      log2fft_size_(frame.log2fft_size_),
      real_data_(frame.fft_size_ / 2),
      imag_data_(frame.fft_size_ / 2),
      context_(nullptr),
      complex_data_(frame.fft_size_),
      pffft_work_(frame.fft_size_) {
  context_ = ContextForSize(fft_size_);

  // Copy/setup frame data.
  unsigned nbytes = sizeof(float) * (fft_size_ / 2);
  memcpy(RealData(), frame.RealData(), nbytes);
  memcpy(ImagData(), frame.ImagData(), nbytes);
}

void FFTFrame::Initialize() {}

void FFTFrame::Cleanup() {}

FFTFrame::~FFTFrame() {
  if (context_)
    pffft_destroy_setup(context_);
}

void FFTFrame::DoFFT(const float* data) {
  DCHECK(context_);
  DCHECK_EQ(complex_data_.size(), fft_size_);

  pffft_transform_ordered(context_, data, complex_data_.Data(),
                          pffft_work_.Data(), PFFFT_FORWARD);

  unsigned len = fft_size_ / 2;

  // Split FFT data into real and imaginary arrays.  PFFFT transform already
  // uses the desired format.
  const float* c = complex_data_.Data();
  float* real = real_data_.Data();
  float* imag = imag_data_.Data();
  for (unsigned k = 0; k < len; ++k) {
    int index = 2 * k;
    real[k] = c[index];
    imag[k] = c[index + 1];
  }
}

void FFTFrame::DoInverseFFT(float* data) {
  DCHECK(context_);
  DCHECK_EQ(complex_data_.size(), fft_size_);

  unsigned len = fft_size_ / 2;

  // Pack the real and imaginary data into the complex array format.  PFFFT
  // already uses the desired format.
  float* fft_data = complex_data_.Data();
  const float* real = real_data_.Data();
  const float* imag = imag_data_.Data();
  for (unsigned k = 0; k < len; ++k) {
    int index = 2 * k;
    fft_data[index] = real[k];
    fft_data[index + 1] = imag[k];
  }

  pffft_transform_ordered(context_, fft_data, data, pffft_work_.Data(),
                          PFFFT_BACKWARD);

  // The inverse transform needs to be scaled because PFFFT doesn't.
  float scale = 1.0 / fft_size_;
  vector_math::Vsmul(data, 1, &scale, data, 1, fft_size_);
}

PFFFT_Setup* FFTFrame::ContextForSize(unsigned fft_size) {
  DCHECK_LE(fft_size, 1U << kMaxFFTPow2Size);

  return pffft_new_setup(fft_size, PFFFT_REAL);
}

}  // namespace blink

#endif  // #if defined(WTF_USE_WEBAUDIO_PFFFT)
