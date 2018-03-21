// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_HISTOGRAMS_TEST_COMMON_H_
#define UI_LATENCY_HISTOGRAMS_TEST_COMMON_H_

#include "ui/latency/fixed_point.h"

#include <array>

namespace ui {
namespace frame_metrics {

// This class initializes the ratio boundaries on construction in a way that
// is easier to follow than the procedural code in the RatioHistogram
// implementation.
class TestRatioBoundaries {
 public:
  TestRatioBoundaries();
  uint64_t operator[](size_t i) const { return boundaries[i]; }
  size_t size() const { return boundaries.size(); }

 public:
  // uint64_t since the last boundary needs 33 bits.
  std::array<uint64_t, 112> boundaries;
};

// An explicit list of VSync boundaries to verify the procedurally generated
// ones in the implementation.
static constexpr std::array<uint32_t, 99> kTestVSyncBoundries = {
    {// C0: [0,1) (1 bucket).
     0,
     // C1: Powers of two from 1 to 2048 us @ 50% precision (12 buckets)
     1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
     // C2: Every 8 Hz from 256 Hz to 128 Hz @ 3-6% precision (16 buckets)
     3906, 4032, 4167, 4310, 4464, 4630, 4808, 5000, 5208, 5435, 5682, 5952,
     6250, 6579, 6944, 7353,
     // C3: Every 4 Hz from 128 Hz to 64 Hz @ 3-6% precision (16 buckets)
     7813, 8065, 8333, 8621, 8929, 9259, 9615, 10000, 10417, 10870, 11364,
     11905, 12500, 13158, 13889, 14706,
     // C4: Every 2 Hz from 64 Hz to 32 Hz @ 3-6% precision (16 buckets)
     15625, 16129, 16667, 17241, 17857, 18519, 19231, 20000, 20833, 21739,
     22727, 23810, 25000, 26316, 27778, 29412,
     // C5: Every 1 Hz from 32 Hz to 1 Hz @ 3-33% precision (31 buckets)
     31250, 32258, 33333, 34483, 35714, 37037, 38462, 40000, 41667, 43478,
     45455, 47619, 50000, 52632, 55556, 58824, 62500, 66667, 71429, 76923,
     83333, 90909, 100000, 111111, 125000, 142857, 166667, 200000, 250000,
     333333, 500000,
     // C6: Powers of two from 1s to 32s @ 50% precision (6 buckets)
     1000000, 2000000, 4000000, 8000000, 16000000, 32000000,
     // C7: Extra value to simplify estimate in Percentiles().
     64000000}};

}  // namespace frame_metrics
}  // namespace ui

#endif  // UI_LATENCY_HISTOGRAMS_TEST_COMMON_H_
