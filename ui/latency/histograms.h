// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_HISTOGRAMS_H_
#define UI_LATENCY_HISTOGRAMS_H_

#include <array>
#include <memory>

#include "base/macros.h"

namespace ui {

// Used to communicate percentile results to clients.
struct PercentileResults {
  static constexpr double kPercentiles[] = {.50, .99};
  static constexpr size_t kCount = arraysize(kPercentiles);

  double values[kCount]{};
};

namespace frame_metrics {

// This is an interface different metrics will use to inject their ideal
// histogram implementations into the StreamAnalyzer.
class Histogram {
 public:
  Histogram() = default;
  virtual ~Histogram() = default;

  // Increases the bucket that contains |value| by |weight|.
  virtual void AddSample(uint32_t value, uint32_t weight) = 0;

  // Calculates and returns the approximate percentiles based on the
  // histogram distribution.
  virtual PercentileResults CalculatePercentiles() const = 0;

  // Resets all buckets in the histogram to 0.
  // Higher level logic may periodically reset the the counts after it
  // gathers the percentiles in order to avoid overflow.
  virtual void Reset() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Histogram);
};

// Ratio histogram, with a range of [0, 2^32) and most of it's precision
// just above kFixedPointMultiplier (i.e. a fixed point of 1).
class RatioHistogram : public Histogram {
 public:
  RatioHistogram();
  ~RatioHistogram() override;
  void AddSample(uint32_t ratio, uint32_t weight) override;
  PercentileResults CalculatePercentiles() const override;
  void Reset() override;

 private:
  static constexpr size_t kBucketCount = 111;

  uint64_t total_samples_ = 0;
  using BucketArray = std::array<uint32_t, kBucketCount>;
  BucketArray buckets_{};

  DISALLOW_COPY_AND_ASSIGN(RatioHistogram);
};

// A histogram of 98 buckets from 0 to 64 seconds with extra precision
// around common vsync boundaries.
class VSyncHistogram : public Histogram {
 public:
  VSyncHistogram();
  ~VSyncHistogram() override;
  void AddSample(uint32_t microseconds, uint32_t weight) override;
  PercentileResults CalculatePercentiles() const override;
  void Reset() override;

 private:
  static constexpr size_t kBucketCount = 98;

  uint64_t total_samples_ = 0;
  using BucketArray = std::array<uint32_t, kBucketCount>;
  BucketArray buckets_{};

  DISALLOW_COPY_AND_ASSIGN(VSyncHistogram);
};

// An interface that allows PercentileHelper to iterate through the
// bucket boundaries of the delegating histogram.
// This is an implemenation detail, but is exposed here for testing purposes.
struct BoundaryIterator {
  virtual ~BoundaryIterator() = default;
  virtual uint64_t Next() = 0;
};

// These expose the internal iterators, so they can be verified in tests.
std::unique_ptr<BoundaryIterator> CreateRatioIteratorForTesting();
std::unique_ptr<BoundaryIterator> CreateVSyncIteratorForTesting();

}  // namespace frame_metrics
}  // namespace ui

#endif  // UI_LATENCY_HISTOGRAMS_H_
