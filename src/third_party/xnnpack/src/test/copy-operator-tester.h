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
#include <limits>
#include <random>
#include <vector>

#include <xnnpack.h>


class CopyOperatorTester {
 public:
  inline CopyOperatorTester& channels(size_t channels) {
    assert(channels != 0);
    this->channels_ = channels;
    return *this;
  }

  inline size_t channels() const {
    return this->channels_;
  }

  inline CopyOperatorTester& input_stride(size_t input_stride) {
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

  inline CopyOperatorTester& output_stride(size_t output_stride) {
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

  inline CopyOperatorTester& batch_size(size_t batch_size) {
    assert(batch_size != 0);
    this->batch_size_ = batch_size;
    return *this;
  }

  inline size_t batch_size() const {
    return this->batch_size_;
  }

  inline CopyOperatorTester& iterations(size_t iterations) {
    this->iterations_ = iterations;
    return *this;
  }

  inline size_t iterations() const {
    return this->iterations_;
  }

  void TestX8() const {
    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    std::uniform_int_distribution<uint32_t> u8dist(
      std::numeric_limits<uint8_t>::min(), std::numeric_limits<uint8_t>::max());

    std::vector<uint8_t> input(XNN_EXTRA_BYTES / sizeof(uint8_t) +
      (batch_size() - 1) * input_stride() + channels());
    std::vector<uint8_t> output((batch_size() - 1) * output_stride() + channels());
    std::vector<uint8_t> output_ref(batch_size() * channels());
    for (size_t iteration = 0; iteration < iterations(); iteration++) {
      std::generate(input.begin(), input.end(), [&]() { return u8dist(rng); });
      std::fill(output.begin(), output.end(), UINT8_C(0xFA));

      // Compute reference results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          output_ref[i * channels() + c] = input[i * input_stride() + c];
        }
      }

      // Create, setup, run, and destroy Copy operator.
      ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
      xnn_operator_t copy_op = nullptr;

      ASSERT_EQ(xnn_status_success,
        xnn_create_copy_nc_x8(
          channels(), input_stride(), output_stride(),
          0, &copy_op));
      ASSERT_NE(nullptr, copy_op);

      // Smart pointer to automatically delete copy_op.
      std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_copy_op(copy_op, xnn_delete_operator);

      ASSERT_EQ(xnn_status_success,
        xnn_setup_copy_nc_x8(
          copy_op,
          batch_size(),
          input.data(), output.data(),
          nullptr /* thread pool */));

      ASSERT_EQ(xnn_status_success,
        xnn_run_operator(copy_op, nullptr /* thread pool */));

      // Verify results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          ASSERT_EQ(output_ref[i * channels() + c], output[i * output_stride() + c])
            << "at batch " << i << " / " << batch_size() << ", channel = " << c << " / " << channels();
        }
      }
    }
  }

  void TestX16() const {
    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    std::uniform_int_distribution<uint16_t> u16dist;

    std::vector<uint16_t> input(XNN_EXTRA_BYTES / sizeof(uint16_t) +
      (batch_size() - 1) * input_stride() + channels());
    std::vector<uint16_t> output((batch_size() - 1) * output_stride() + channels());
    std::vector<uint16_t> output_ref(batch_size() * channels());
    for (size_t iteration = 0; iteration < iterations(); iteration++) {
      std::generate(input.begin(), input.end(), [&]() { return u16dist(rng); });
      std::fill(output.begin(), output.end(), UINT16_C(0xDEAD));

      // Compute reference results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          output_ref[i * channels() + c] = input[i * input_stride() + c];
        }
      }

      // Create, setup, run, and destroy Copy operator.
      ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
      xnn_operator_t copy_op = nullptr;

      ASSERT_EQ(xnn_status_success,
        xnn_create_copy_nc_x16(
          channels(), input_stride(), output_stride(),
          0, &copy_op));
      ASSERT_NE(nullptr, copy_op);

      // Smart pointer to automatically delete copy_op.
      std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_copy_op(copy_op, xnn_delete_operator);

      ASSERT_EQ(xnn_status_success,
        xnn_setup_copy_nc_x16(
          copy_op,
          batch_size(),
          input.data(), output.data(),
          nullptr /* thread pool */));

      ASSERT_EQ(xnn_status_success,
        xnn_run_operator(copy_op, nullptr /* thread pool */));

      // Verify results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          ASSERT_EQ(output_ref[i * channels() + c], output[i * output_stride() + c])
            << "at batch " << i << " / " << batch_size() << ", channel = " << c << " / " << channels();
        }
      }
    }
  }

  void TestX32() const {
    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    std::uniform_int_distribution<uint32_t> u32dist;

    std::vector<uint32_t> input(XNN_EXTRA_BYTES / sizeof(uint32_t) +
      (batch_size() - 1) * input_stride() + channels());
    std::vector<uint32_t> output((batch_size() - 1) * output_stride() + channels());
    std::vector<uint32_t> output_ref(batch_size() * channels());
    for (size_t iteration = 0; iteration < iterations(); iteration++) {
      std::generate(input.begin(), input.end(), [&]() { return u32dist(rng); });
      std::fill(output.begin(), output.end(), UINT32_C(0xDEADBEEF));

      // Compute reference results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          output_ref[i * channels() + c] = input[i * input_stride() + c];
        }
      }

      // Create, setup, run, and destroy Copy operator.
      ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
      xnn_operator_t copy_op = nullptr;

      ASSERT_EQ(xnn_status_success,
        xnn_create_copy_nc_x32(
          channels(), input_stride(), output_stride(),
          0, &copy_op));
      ASSERT_NE(nullptr, copy_op);

      // Smart pointer to automatically delete copy_op.
      std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_copy_op(copy_op, xnn_delete_operator);

      ASSERT_EQ(xnn_status_success,
        xnn_setup_copy_nc_x32(
          copy_op,
          batch_size(),
          input.data(), output.data(),
          nullptr /* thread pool */));

      ASSERT_EQ(xnn_status_success,
        xnn_run_operator(copy_op, nullptr /* thread pool */));

      // Verify results.
      for (size_t i = 0; i < batch_size(); i++) {
        for (size_t c = 0; c < channels(); c++) {
          ASSERT_EQ(output_ref[i * channels() + c], output[i * output_stride() + c])
            << "at batch " << i << " / " << batch_size() << ", channel = " << c << " / " << channels();
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
