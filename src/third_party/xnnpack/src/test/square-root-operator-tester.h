// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <random>
#include <vector>

#include <fp16.h>

#include <xnnpack.h>


class SquareRootOperatorTester {
 public:
  inline SquareRootOperatorTester& channels(size_t channels) {
    assert(channels != 0);
    this->channels_ = channels;
    return *this;
  }

  inline size_t channels() const {
    return this->channels_;
  }

  inline SquareRootOperatorTester& input_stride(size_t input_stride) {
    assert(input_stride != 0);
    this->input_stride_ = input_stride;
    return *this;
  }

  inline size_t input_stride() const {
    if (this->input_stride_ == 0) {
      return this->channels_;
    } else {
      assert(this->input_stride_ >= this->channels_);
      return this->input_stride_;
    }
  }

  inline SquareRootOperatorTester& output_stride(size_t output_stride) {
    assert(output_stride != 0);
    this->output_stride_ = output_stride;
    return *this;
  }

  inline size_t output_stride() const {
    if (this->output_stride_ == 0) {
      return this->channels_;
    } else {
      assert(this->output_stride_ >= this->channels_);
      return this->output_stride_;
    }
  }

  inline SquareRootOperatorTester& batch_size(size_t batch_size) {
    assert(batch_size != 0);
    this->batch_size_ = batch_size;
    return *this;
  }

  inline size_t batch_size() const {
    return this->batch_size_;
  }

  inline SquareRootOperatorTester& iterations(size_t iterations) {
    this->iterations_ = iterations;
    return *this;
  }

  inline size_t iterations() const {
    return this->iterations_;
  }

  void TestF16() const {
    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    std::uniform_real_distribution<float> f32dist(0.1f, 5.0f);

    std::vector<uint16_t> input(XNN_EXTRA_BYTES / sizeof(uint16_t) +
      (batch_size() - 1) * input_stride() + channels());
    std::vector<uint16_t> output((batch_size() - 1) * output_stride() + channels());
    std::vector<float> output_ref(batch_size() * channels());
    for (size_t iteration = 0; iteration < iterations(); iteration++) {
      std::generate(input.begin(), input.end(), [&]() { return fp16_ieee_from_fp32_value(f32dist(rng)); });
      std::fill(output.begin(), output.end(), UINT16_C(0x7E00) /* NaN */);

      // Compute reference results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          output_ref[i * channels() + c] = std::sqrt(fp16_ieee_to_fp32_value(input[i * input_stride() + c]));
        }
      }

      // Create, setup, run, and destroy Square operator.
      ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
      xnn_operator_t sqrt_op = nullptr;

      const xnn_status status = xnn_create_square_root_nc_f16(
        channels(), input_stride(), output_stride(),
        0, &sqrt_op);
      if (status == xnn_status_unsupported_hardware) {
        GTEST_SKIP();
      }
      ASSERT_EQ(xnn_status_success, status);
      ASSERT_NE(nullptr, sqrt_op);

      // Smart pointer to automatically delete sqrt_op.
      std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_sqrt_op(sqrt_op, xnn_delete_operator);

      ASSERT_EQ(xnn_status_success,
        xnn_setup_square_root_nc_f16(
          sqrt_op,
          batch_size(),
          input.data(), output.data(),
          nullptr /* thread pool */));

      ASSERT_EQ(xnn_status_success,
        xnn_run_operator(sqrt_op, nullptr /* thread pool */));

      // Verify results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          ASSERT_NEAR(
              fp16_ieee_to_fp32_value(output[i * output_stride() + c]),
              output_ref[i * channels() + c],
              std::abs(output_ref[i * channels() + c]) * 5.0e-3f)
            << "at batch " << i << " / " << batch_size() << ", channel " << c << " / " << channels()
            << ", input " << fp16_ieee_to_fp32_value(input[i * input_stride() + c]);
        }
      }
    }
  }

  void TestF32() const {
    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    std::uniform_real_distribution<float> f32dist(0.0f, 5.0f);

    std::vector<float> input(XNN_EXTRA_BYTES / sizeof(float) +
      (batch_size() - 1) * input_stride() + channels());
    std::vector<float> output((batch_size() - 1) * output_stride() + channels());
    std::vector<float> output_ref(batch_size() * channels());
    for (size_t iteration = 0; iteration < iterations(); iteration++) {
      std::generate(input.begin(), input.end(), [&]() { return f32dist(rng); });
      std::fill(output.begin(), output.end(), std::nanf(""));

      // Compute reference results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          output_ref[i * channels() + c] = std::sqrt(input[i * input_stride() + c]);
        }
      }

      // Create, setup, run, and destroy Square operator.
      ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
      xnn_operator_t sqrt_op = nullptr;

      ASSERT_EQ(xnn_status_success,
        xnn_create_square_root_nc_f32(
          channels(), input_stride(), output_stride(),
          0, &sqrt_op));
      ASSERT_NE(nullptr, sqrt_op);

      // Smart pointer to automatically delete sqrt_op.
      std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_sqrt_op(sqrt_op, xnn_delete_operator);

      ASSERT_EQ(xnn_status_success,
        xnn_setup_square_root_nc_f32(
          sqrt_op,
          batch_size(),
          input.data(), output.data(),
          nullptr /* thread pool */));

      ASSERT_EQ(xnn_status_success,
        xnn_run_operator(sqrt_op, nullptr /* thread pool */));

      // Verify results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          ASSERT_EQ(output_ref[i * channels() + c], output[i * output_stride() + c])
            << "at batch " << i << " / " << batch_size() << ", channel " << c << " / " << channels()
            << ", input " << input[i * input_stride() + c];
        }
      }
    }
  }

 private:
  size_t batch_size_{1};
  size_t channels_{1};
  size_t input_stride_{0};
  size_t output_stride_{0};
  size_t iterations_{15};
};
