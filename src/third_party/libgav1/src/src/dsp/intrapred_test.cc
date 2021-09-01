// Copyright 2020 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/dsp/intrapred.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>

#include "absl/strings/match.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/cpu.h"
#include "src/utils/memory.h"
#include "tests/block_utils.h"
#include "tests/third_party/libvpx/acm_random.h"
#include "tests/utils.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr int kMaxBlockSize = 64;
constexpr int kTotalPixels = kMaxBlockSize * kMaxBlockSize;
constexpr int kNumDirectionalIntraPredictors = 3;

constexpr int kBaseAngles[] = {45, 67, 90, 113, 135, 157, 180, 203};

const char* const kFilterIntraPredNames[kNumFilterIntraPredictors] = {
    "kFilterIntraPredictorDc",         "kFilterIntraPredictorVertical",
    "kFilterIntraPredictorHorizontal", "kFilterIntraPredictorD157",
    "kFilterIntraPredictorPaeth",
};

const char* const kCflIntraPredName = "kCflIntraPredictor";

const char* const kDirectionalPredNames[kNumDirectionalIntraPredictors] = {
    "kDirectionalIntraPredictorZone1", "kDirectionalIntraPredictorZone2",
    "kDirectionalIntraPredictorZone3"};

const char* const kTransformSizeNames[kNumTransformSizes] = {
    "kTransformSize4x4",   "kTransformSize4x8",   "kTransformSize4x16",
    "kTransformSize8x4",   "kTransformSize8x8",   "kTransformSize8x16",
    "kTransformSize8x32",  "kTransformSize16x4",  "kTransformSize16x8",
    "kTransformSize16x16", "kTransformSize16x32", "kTransformSize16x64",
    "kTransformSize32x8",  "kTransformSize32x16", "kTransformSize32x32",
    "kTransformSize32x64", "kTransformSize64x16", "kTransformSize64x32",
    "kTransformSize64x64",
};

int16_t GetDirectionalIntraPredictorDerivative(const int angle) {
  EXPECT_GE(angle, 3);
  EXPECT_LE(angle, 87);
  return kDirectionalIntraPredictorDerivative[DivideBy2(angle) - 1];
}

template <int bitdepth, typename Pixel>
class IntraPredTestBase : public ::testing::TestWithParam<TransformSize>,
                          public test_utils::MaxAlignedAllocable {
 public:
  IntraPredTestBase() {
    switch (tx_size_) {
      case kNumTransformSizes:
        EXPECT_NE(tx_size_, kNumTransformSizes);
        break;
      default:
        block_width_ = kTransformWidth[tx_size_];
        block_height_ = kTransformHeight[tx_size_];
        break;
    }
  }

  IntraPredTestBase(const IntraPredTestBase&) = delete;
  IntraPredTestBase& operator=(const IntraPredTestBase&) = delete;
  ~IntraPredTestBase() override = default;

 protected:
  struct IntraPredMem {
    void Reset(libvpx_test::ACMRandom* rnd) {
      ASSERT_NE(rnd, nullptr);
      Pixel* const left = left_mem + 16;
      Pixel* const top = top_mem + 16;
      const int mask = (1 << bitdepth) - 1;
      for (auto& r : ref_src) r = rnd->Rand16() & mask;
      for (int i = 0; i < kMaxBlockSize; ++i) left[i] = rnd->Rand16() & mask;
      for (int i = -1; i < kMaxBlockSize; ++i) top[i] = rnd->Rand16() & mask;

      // Some directional predictors require top-right, bottom-left.
      for (int i = kMaxBlockSize; i < 2 * kMaxBlockSize; ++i) {
        left[i] = rnd->Rand16() & mask;
        top[i] = rnd->Rand16() & mask;
      }
      // TODO(jzern): reorder this and regenerate the digests after switching
      // random number generators.
      // Upsampling in the directional predictors extends left/top[-1] to [-2].
      left[-1] = rnd->Rand16() & mask;
      left[-2] = rnd->Rand16() & mask;
      top[-2] = rnd->Rand16() & mask;
      memset(left_mem, 0, sizeof(left_mem[0]) * 14);
      memset(top_mem, 0, sizeof(top_mem[0]) * 14);
      memset(top_mem + kMaxBlockSize * 2 + 16, 0,
             sizeof(top_mem[0]) * kTopMemPadding);
    }

    // Set ref_src, top-left, top and left to |pixel|.
    void Set(const Pixel pixel) {
      Pixel* const left = left_mem + 16;
      Pixel* const top = top_mem + 16;
      for (auto& r : ref_src) r = pixel;
      // Upsampling in the directional predictors extends left/top[-1] to [-2].
      for (int i = -2; i < 2 * kMaxBlockSize; ++i) {
        left[i] = top[i] = pixel;
      }
    }

    // DirectionalZone1_Large() overreads up to 7 pixels in |top_mem|.
    static constexpr int kTopMemPadding = 7;
    alignas(kMaxAlignment) Pixel dst[kTotalPixels];
    alignas(kMaxAlignment) Pixel ref_src[kTotalPixels];
    alignas(kMaxAlignment) Pixel left_mem[kMaxBlockSize * 2 + 16];
    alignas(
        kMaxAlignment) Pixel top_mem[kMaxBlockSize * 2 + 16 + kTopMemPadding];
  };

  void SetUp() override { test_utils::ResetDspTable(bitdepth); }

  const TransformSize tx_size_ = GetParam();
  int block_width_;
  int block_height_;
  IntraPredMem intra_pred_mem_;
};

//------------------------------------------------------------------------------
// IntraPredTest

template <int bitdepth, typename Pixel>
class IntraPredTest : public IntraPredTestBase<bitdepth, Pixel> {
 public:
  IntraPredTest() = default;
  IntraPredTest(const IntraPredTest&) = delete;
  IntraPredTest& operator=(const IntraPredTest&) = delete;
  ~IntraPredTest() override = default;

 protected:
  using IntraPredTestBase<bitdepth, Pixel>::tx_size_;
  using IntraPredTestBase<bitdepth, Pixel>::block_width_;
  using IntraPredTestBase<bitdepth, Pixel>::block_height_;
  using IntraPredTestBase<bitdepth, Pixel>::intra_pred_mem_;

  void SetUp() override {
    IntraPredTestBase<bitdepth, Pixel>::SetUp();
    IntraPredInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    memcpy(base_intrapreds_, dsp->intra_predictors[tx_size_],
           sizeof(base_intrapreds_));

    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      memset(base_intrapreds_, 0, sizeof(base_intrapreds_));
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        IntraPredInit_SSE4_1();
        IntraPredSmoothInit_SSE4_1();
      }
    } else if (absl::StartsWith(test_case, "NEON/")) {
      IntraPredInit_NEON();
      IntraPredSmoothInit_NEON();
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }

    memcpy(cur_intrapreds_, dsp->intra_predictors[tx_size_],
           sizeof(cur_intrapreds_));

    for (int i = 0; i < kNumIntraPredictors; ++i) {
      // skip functions that haven't been specialized for this particular
      // architecture.
      if (cur_intrapreds_[i] == base_intrapreds_[i]) {
        cur_intrapreds_[i] = nullptr;
      }
    }
  }

  // These tests modify intra_pred_mem_.
  void TestSpeed(const char* const digests[kNumIntraPredictors], int num_runs);
  void TestSaturatedValues();
  void TestRandomValues();

  IntraPredictorFunc base_intrapreds_[kNumIntraPredictors];
  IntraPredictorFunc cur_intrapreds_[kNumIntraPredictors];
};

template <int bitdepth, typename Pixel>
void IntraPredTest<bitdepth, Pixel>::TestSpeed(
    const char* const digests[kNumIntraPredictors], const int num_runs) {
  ASSERT_NE(digests, nullptr);
  const auto* const left =
      reinterpret_cast<const uint8_t*>(intra_pred_mem_.left_mem + 16);
  const auto* const top =
      reinterpret_cast<const uint8_t*>(intra_pred_mem_.top_mem + 16);

  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  intra_pred_mem_.Reset(&rnd);

  for (int i = 0; i < kNumIntraPredictors; ++i) {
    if (cur_intrapreds_[i] == nullptr) continue;
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const absl::Time start = absl::Now();
    for (int run = 0; run < num_runs; ++run) {
      const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
      cur_intrapreds_[i](intra_pred_mem_.dst, stride, top, left);
    }
    const absl::Duration elapsed_time = absl::Now() - start;
    test_utils::CheckMd5Digest(kTransformSizeNames[tx_size_],
                               ToString(static_cast<IntraPredictor>(i)),
                               digests[i], intra_pred_mem_.dst,
                               sizeof(intra_pred_mem_.dst), elapsed_time);
  }
}

template <int bitdepth, typename Pixel>
void IntraPredTest<bitdepth, Pixel>::TestSaturatedValues() {
  Pixel* const left = intra_pred_mem_.left_mem + 16;
  Pixel* const top = intra_pred_mem_.top_mem + 16;
  const auto kMaxPixel = static_cast<Pixel>((1 << bitdepth) - 1);
  intra_pred_mem_.Set(kMaxPixel);

  // skip DcFill
  for (int i = 1; i < kNumIntraPredictors; ++i) {
    if (cur_intrapreds_[i] == nullptr) continue;
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
    cur_intrapreds_[i](intra_pred_mem_.dst, stride, top, left);
    if (!test_utils::CompareBlocks(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
                                   block_width_, block_height_, kMaxBlockSize,
                                   kMaxBlockSize, true)) {
      ADD_FAILURE() << "Expected " << ToString(static_cast<IntraPredictor>(i))
                    << " to produce a block containing '"
                    << static_cast<int>(kMaxPixel) << "'";
    }
  }
}

template <int bitdepth, typename Pixel>
void IntraPredTest<bitdepth, Pixel>::TestRandomValues() {
  // Use an alternate seed to differentiate this test from TestSpeed().
  libvpx_test::ACMRandom rnd(test_utils::kAlternateDeterministicSeed);
  for (int i = 0; i < kNumIntraPredictors; ++i) {
    // Skip the 'C' test case as this is used as the reference.
    if (base_intrapreds_[i] == nullptr) continue;
    if (cur_intrapreds_[i] == nullptr) continue;
    // It may be worthwhile to temporarily increase this loop size when testing
    // changes that specifically affect this test.
    for (int n = 0; n < 10000; ++n) {
      intra_pred_mem_.Reset(&rnd);

      memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
             sizeof(intra_pred_mem_.dst));
      const Pixel* const top = intra_pred_mem_.top_mem + 16;
      const Pixel* const left = intra_pred_mem_.left_mem + 16;
      const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
      base_intrapreds_[i](intra_pred_mem_.ref_src, stride, top, left);
      cur_intrapreds_[i](intra_pred_mem_.dst, stride, top, left);
      if (!test_utils::CompareBlocks(
              intra_pred_mem_.dst, intra_pred_mem_.ref_src, block_width_,
              block_height_, kMaxBlockSize, kMaxBlockSize, true)) {
        ADD_FAILURE() << "Result from optimized version of "
                      << ToString(static_cast<IntraPredictor>(i))
                      << " differs from reference in iteration #" << n;
        break;
      }
    }
  }
}

//------------------------------------------------------------------------------
// DirectionalIntraPredTest

template <int bitdepth, typename Pixel>
class DirectionalIntraPredTest : public IntraPredTestBase<bitdepth, Pixel> {
 public:
  DirectionalIntraPredTest() = default;
  DirectionalIntraPredTest(const DirectionalIntraPredTest&) = delete;
  DirectionalIntraPredTest& operator=(const DirectionalIntraPredTest&) = delete;
  ~DirectionalIntraPredTest() override = default;

 protected:
  using IntraPredTestBase<bitdepth, Pixel>::tx_size_;
  using IntraPredTestBase<bitdepth, Pixel>::block_width_;
  using IntraPredTestBase<bitdepth, Pixel>::block_height_;
  using IntraPredTestBase<bitdepth, Pixel>::intra_pred_mem_;

  enum Zone { kZone1, kZone2, kZone3, kNumZones };

  enum { kAngleDeltaStart = -9, kAngleDeltaStop = 9, kAngleDeltaStep = 3 };

  void SetUp() override {
    IntraPredTestBase<bitdepth, Pixel>::SetUp();
    IntraPredInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_directional_intra_pred_zone1_ = dsp->directional_intra_predictor_zone1;
    base_directional_intra_pred_zone2_ = dsp->directional_intra_predictor_zone2;
    base_directional_intra_pred_zone3_ = dsp->directional_intra_predictor_zone3;

    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      base_directional_intra_pred_zone1_ = nullptr;
      base_directional_intra_pred_zone2_ = nullptr;
      base_directional_intra_pred_zone3_ = nullptr;
    } else if (absl::StartsWith(test_case, "NEON/")) {
      IntraPredDirectionalInit_NEON();
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        IntraPredInit_SSE4_1();
      }
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }

    cur_directional_intra_pred_zone1_ = dsp->directional_intra_predictor_zone1;
    cur_directional_intra_pred_zone2_ = dsp->directional_intra_predictor_zone2;
    cur_directional_intra_pred_zone3_ = dsp->directional_intra_predictor_zone3;

    // Skip functions that haven't been specialized for this particular
    // architecture.
    if (cur_directional_intra_pred_zone1_ ==
        base_directional_intra_pred_zone1_) {
      cur_directional_intra_pred_zone1_ = nullptr;
    }
    if (cur_directional_intra_pred_zone2_ ==
        base_directional_intra_pred_zone2_) {
      cur_directional_intra_pred_zone2_ = nullptr;
    }
    if (cur_directional_intra_pred_zone3_ ==
        base_directional_intra_pred_zone3_) {
      cur_directional_intra_pred_zone3_ = nullptr;
    }
  }

  bool IsEdgeUpsampled(int delta, const int filter_type) const {
    delta = std::abs(delta);
    if (delta == 0 || delta >= 40) return false;
    const int block_wh = block_width_ + block_height_;
    return (filter_type == 1) ? block_wh <= 8 : block_wh <= 16;
  }

  // Returns the minimum and maximum (exclusive) range of angles that the
  // predictor should be applied to.
  void GetZoneAngleRange(const Zone zone, int* const min_angle,
                         int* const max_angle) const {
    ASSERT_NE(min_angle, nullptr);
    ASSERT_NE(max_angle, nullptr);
    switch (zone) {
      case kZone1:
        *min_angle = 0;
        *max_angle = 90;
        break;
      case kZone2:
        *min_angle = 90;
        *max_angle = 180;
        break;
      case kZone3:
        *min_angle = 180;
        *max_angle = 270;
        break;
      case kNumZones:
        FAIL() << "Invalid zone value: " << zone;
        break;
    }
  }

  // These tests modify intra_pred_mem_.
  void TestSpeed(const char* const digests[kNumDirectionalIntraPredictors],
                 Zone zone, int num_runs);
  void TestSaturatedValues();
  void TestRandomValues();

  DirectionalIntraPredictorZone1Func base_directional_intra_pred_zone1_;
  DirectionalIntraPredictorZone2Func base_directional_intra_pred_zone2_;
  DirectionalIntraPredictorZone3Func base_directional_intra_pred_zone3_;
  DirectionalIntraPredictorZone1Func cur_directional_intra_pred_zone1_;
  DirectionalIntraPredictorZone2Func cur_directional_intra_pred_zone2_;
  DirectionalIntraPredictorZone3Func cur_directional_intra_pred_zone3_;
};

template <int bitdepth, typename Pixel>
void DirectionalIntraPredTest<bitdepth, Pixel>::TestSpeed(
    const char* const digests[kNumDirectionalIntraPredictors], const Zone zone,
    const int num_runs) {
  switch (zone) {
    case kZone1:
      if (cur_directional_intra_pred_zone1_ == nullptr) return;
      break;
    case kZone2:
      if (cur_directional_intra_pred_zone2_ == nullptr) return;
      break;
    case kZone3:
      if (cur_directional_intra_pred_zone3_ == nullptr) return;
      break;
    case kNumZones:
      FAIL() << "Invalid zone value: " << zone;
      break;
  }
  ASSERT_NE(digests, nullptr);
  const Pixel* const left = intra_pred_mem_.left_mem + 16;
  const Pixel* const top = intra_pred_mem_.top_mem + 16;

  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  intra_pred_mem_.Reset(&rnd);

  // Allocate separate blocks for each angle + filter + upsampled combination.
  // Add a 1 pixel right border to test for overwrites.
  static constexpr int kMaxZoneAngles = 27;  // zone 2
  static constexpr int kMaxFilterTypes = 2;
  static constexpr int kBlockBorder = 1;
  static constexpr int kBorderSize =
      kBlockBorder * kMaxZoneAngles * kMaxFilterTypes;
  const int ref_stride =
      kMaxZoneAngles * kMaxFilterTypes * block_width_ + kBorderSize;
  const size_t ref_alloc_size = sizeof(Pixel) * ref_stride * block_height_;

  using AlignedPtr = std::unique_ptr<Pixel[], decltype(&AlignedFree)>;
  AlignedPtr ref_src(static_cast<Pixel*>(AlignedAlloc(16, ref_alloc_size)),
                     &AlignedFree);
  AlignedPtr dest(static_cast<Pixel*>(AlignedAlloc(16, ref_alloc_size)),
                  &AlignedFree);
  ASSERT_NE(ref_src, nullptr);
  ASSERT_NE(dest, nullptr);

  const int mask = (1 << bitdepth) - 1;
  for (size_t i = 0; i < ref_alloc_size / sizeof(ref_src[0]); ++i) {
    ref_src[i] = rnd.Rand16() & mask;
  }

  int min_angle = 0, max_angle = 0;
  ASSERT_NO_FATAL_FAILURE(GetZoneAngleRange(zone, &min_angle, &max_angle));

  absl::Duration elapsed_time;
  for (int run = 0; run < num_runs; ++run) {
    Pixel* dst = dest.get();
    memcpy(dst, ref_src.get(), ref_alloc_size);
    for (const auto& base_angle : kBaseAngles) {
      for (int filter_type = 0; filter_type <= 1; ++filter_type) {
        for (int angle_delta = kAngleDeltaStart; angle_delta <= kAngleDeltaStop;
             angle_delta += kAngleDeltaStep) {
          const int predictor_angle = base_angle + angle_delta;
          if (predictor_angle <= min_angle || predictor_angle >= max_angle) {
            continue;
          }

          ASSERT_GT(predictor_angle, 0) << "base_angle: " << base_angle
                                        << " angle_delta: " << angle_delta;
          const bool upsampled_left =
              IsEdgeUpsampled(predictor_angle - 180, filter_type);
          const bool upsampled_top =
              IsEdgeUpsampled(predictor_angle - 90, filter_type);
          const ptrdiff_t stride = ref_stride * sizeof(ref_src[0]);
          if (predictor_angle < 90) {
            ASSERT_EQ(zone, kZone1);
            const int xstep =
                GetDirectionalIntraPredictorDerivative(predictor_angle);
            const absl::Time start = absl::Now();
            cur_directional_intra_pred_zone1_(dst, stride, top, block_width_,
                                              block_height_, xstep,
                                              upsampled_top);
            elapsed_time += absl::Now() - start;
          } else if (predictor_angle < 180) {
            ASSERT_EQ(zone, kZone2);
            const int xstep =
                GetDirectionalIntraPredictorDerivative(180 - predictor_angle);
            const int ystep =
                GetDirectionalIntraPredictorDerivative(predictor_angle - 90);
            const absl::Time start = absl::Now();
            cur_directional_intra_pred_zone2_(
                dst, stride, top, left, block_width_, block_height_, xstep,
                ystep, upsampled_top, upsampled_left);
            elapsed_time += absl::Now() - start;
          } else {
            ASSERT_EQ(zone, kZone3);
            ASSERT_LT(predictor_angle, 270);
            const int ystep =
                GetDirectionalIntraPredictorDerivative(270 - predictor_angle);
            const absl::Time start = absl::Now();
            cur_directional_intra_pred_zone3_(dst, stride, left, block_width_,
                                              block_height_, ystep,
                                              upsampled_left);
            elapsed_time += absl::Now() - start;
          }
          dst += block_width_ + kBlockBorder;
        }
      }
    }
  }

  test_utils::CheckMd5Digest(kTransformSizeNames[tx_size_],
                             kDirectionalPredNames[zone], digests[zone],
                             dest.get(), ref_alloc_size, elapsed_time);
}

template <int bitdepth, typename Pixel>
void DirectionalIntraPredTest<bitdepth, Pixel>::TestSaturatedValues() {
  const Pixel* const left = intra_pred_mem_.left_mem + 16;
  const Pixel* const top = intra_pred_mem_.top_mem + 16;
  const auto kMaxPixel = static_cast<Pixel>((1 << bitdepth) - 1);
  intra_pred_mem_.Set(kMaxPixel);

  for (int i = kZone1; i < kNumZones; ++i) {
    switch (i) {
      case kZone1:
        if (cur_directional_intra_pred_zone1_ == nullptr) continue;
        break;
      case kZone2:
        if (cur_directional_intra_pred_zone2_ == nullptr) continue;
        break;
      case kZone3:
        if (cur_directional_intra_pred_zone3_ == nullptr) continue;
        break;
      case kNumZones:
        FAIL() << "Invalid zone value: " << i;
        break;
    }
    int min_angle = 0, max_angle = 0;
    ASSERT_NO_FATAL_FAILURE(
        GetZoneAngleRange(static_cast<Zone>(i), &min_angle, &max_angle));

    for (const auto& base_angle : kBaseAngles) {
      for (int filter_type = 0; filter_type <= 1; ++filter_type) {
        for (int angle_delta = kAngleDeltaStart; angle_delta <= kAngleDeltaStop;
             angle_delta += kAngleDeltaStep) {
          const int predictor_angle = base_angle + angle_delta;
          if (predictor_angle <= min_angle || predictor_angle >= max_angle) {
            continue;
          }
          ASSERT_GT(predictor_angle, 0) << "base_angle: " << base_angle
                                        << " angle_delta: " << angle_delta;

          memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
                 sizeof(intra_pred_mem_.dst));

          const bool upsampled_left =
              IsEdgeUpsampled(predictor_angle - 180, filter_type);
          const bool upsampled_top =
              IsEdgeUpsampled(predictor_angle - 90, filter_type);
          const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
          if (predictor_angle < 90) {
            const int xstep =
                GetDirectionalIntraPredictorDerivative(predictor_angle);
            cur_directional_intra_pred_zone1_(intra_pred_mem_.dst, stride, top,
                                              block_width_, block_height_,
                                              xstep, upsampled_top);
          } else if (predictor_angle < 180) {
            const int xstep =
                GetDirectionalIntraPredictorDerivative(180 - predictor_angle);
            const int ystep =
                GetDirectionalIntraPredictorDerivative(predictor_angle - 90);
            cur_directional_intra_pred_zone2_(
                intra_pred_mem_.dst, stride, top, left, block_width_,
                block_height_, xstep, ystep, upsampled_top, upsampled_left);
          } else {
            ASSERT_LT(predictor_angle, 270);
            const int ystep =
                GetDirectionalIntraPredictorDerivative(270 - predictor_angle);
            cur_directional_intra_pred_zone3_(intra_pred_mem_.dst, stride, left,
                                              block_width_, block_height_,
                                              ystep, upsampled_left);
          }

          if (!test_utils::CompareBlocks(
                  intra_pred_mem_.dst, intra_pred_mem_.ref_src, block_width_,
                  block_height_, kMaxBlockSize, kMaxBlockSize, true)) {
            ADD_FAILURE() << "Expected " << kDirectionalPredNames[i]
                          << " (angle: " << predictor_angle
                          << " filter type: " << filter_type
                          << ") to produce a block containing '"
                          << static_cast<int>(kMaxPixel) << "'";
            return;
          }
        }
      }
    }
  }
}

template <int bitdepth, typename Pixel>
void DirectionalIntraPredTest<bitdepth, Pixel>::TestRandomValues() {
  const Pixel* const left = intra_pred_mem_.left_mem + 16;
  const Pixel* const top = intra_pred_mem_.top_mem + 16;
  // Use an alternate seed to differentiate this test from TestSpeed().
  libvpx_test::ACMRandom rnd(test_utils::kAlternateDeterministicSeed);

  for (int i = kZone1; i < kNumZones; ++i) {
    // Only run when there is a reference version (base) and a different
    // optimized version (cur).
    switch (i) {
      case kZone1:
        if (base_directional_intra_pred_zone1_ == nullptr ||
            cur_directional_intra_pred_zone1_ == nullptr) {
          continue;
        }
        break;
      case kZone2:
        if (base_directional_intra_pred_zone2_ == nullptr ||
            cur_directional_intra_pred_zone2_ == nullptr) {
          continue;
        }
        break;
      case kZone3:
        if (base_directional_intra_pred_zone3_ == nullptr ||
            cur_directional_intra_pred_zone3_ == nullptr) {
          continue;
        }
        break;
      case kNumZones:
        FAIL() << "Invalid zone value: " << i;
        break;
    }
    int min_angle = 0, max_angle = 0;
    ASSERT_NO_FATAL_FAILURE(
        GetZoneAngleRange(static_cast<Zone>(i), &min_angle, &max_angle));

    for (const auto& base_angle : kBaseAngles) {
      for (int n = 0; n < 1000; ++n) {
        for (int filter_type = 0; filter_type <= 1; ++filter_type) {
          for (int angle_delta = kAngleDeltaStart;
               angle_delta <= kAngleDeltaStop; angle_delta += kAngleDeltaStep) {
            const int predictor_angle = base_angle + angle_delta;
            if (predictor_angle <= min_angle || predictor_angle >= max_angle) {
              continue;
            }
            ASSERT_GT(predictor_angle, 0) << "base_angle: " << base_angle
                                          << " angle_delta: " << angle_delta;

            intra_pred_mem_.Reset(&rnd);
            memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
                   sizeof(intra_pred_mem_.dst));

            const bool upsampled_left =
                IsEdgeUpsampled(predictor_angle - 180, filter_type);
            const bool upsampled_top =
                IsEdgeUpsampled(predictor_angle - 90, filter_type);
            const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
            if (predictor_angle < 90) {
              const int xstep =
                  GetDirectionalIntraPredictorDerivative(predictor_angle);
              base_directional_intra_pred_zone1_(
                  intra_pred_mem_.ref_src, stride, top, block_width_,
                  block_height_, xstep, upsampled_top);
              cur_directional_intra_pred_zone1_(
                  intra_pred_mem_.dst, stride, top, block_width_, block_height_,
                  xstep, upsampled_top);
            } else if (predictor_angle < 180) {
              const int xstep =
                  GetDirectionalIntraPredictorDerivative(180 - predictor_angle);
              const int ystep =
                  GetDirectionalIntraPredictorDerivative(predictor_angle - 90);
              base_directional_intra_pred_zone2_(
                  intra_pred_mem_.ref_src, stride, top, left, block_width_,
                  block_height_, xstep, ystep, upsampled_top, upsampled_left);
              cur_directional_intra_pred_zone2_(
                  intra_pred_mem_.dst, stride, top, left, block_width_,
                  block_height_, xstep, ystep, upsampled_top, upsampled_left);
            } else {
              ASSERT_LT(predictor_angle, 270);
              const int ystep =
                  GetDirectionalIntraPredictorDerivative(270 - predictor_angle);
              base_directional_intra_pred_zone3_(
                  intra_pred_mem_.ref_src, stride, left, block_width_,
                  block_height_, ystep, upsampled_left);
              cur_directional_intra_pred_zone3_(
                  intra_pred_mem_.dst, stride, left, block_width_,
                  block_height_, ystep, upsampled_left);
            }

            if (!test_utils::CompareBlocks(
                    intra_pred_mem_.dst, intra_pred_mem_.ref_src, block_width_,
                    block_height_, kMaxBlockSize, kMaxBlockSize, true)) {
              ADD_FAILURE() << "Result from optimized version of "
                            << kDirectionalPredNames[i]
                            << " differs from reference at angle "
                            << predictor_angle << " with filter type "
                            << filter_type << " in iteration #" << n;
              return;
            }
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------
// FilterIntraPredTest

template <int bitdepth, typename Pixel>
class FilterIntraPredTest : public IntraPredTestBase<bitdepth, Pixel> {
 public:
  FilterIntraPredTest() = default;
  FilterIntraPredTest(const FilterIntraPredTest&) = delete;
  FilterIntraPredTest& operator=(const FilterIntraPredTest&) = delete;
  ~FilterIntraPredTest() override = default;

 protected:
  using IntraPredTestBase<bitdepth, Pixel>::tx_size_;
  using IntraPredTestBase<bitdepth, Pixel>::block_width_;
  using IntraPredTestBase<bitdepth, Pixel>::block_height_;
  using IntraPredTestBase<bitdepth, Pixel>::intra_pred_mem_;

  void SetUp() override {
    IntraPredTestBase<bitdepth, Pixel>::SetUp();
    IntraPredInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_filter_intra_pred_ = dsp->filter_intra_predictor;

    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      // No need to compare C with itself.
      base_filter_intra_pred_ = nullptr;
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        IntraPredInit_SSE4_1();
      }
    } else if (absl::StartsWith(test_case, "NEON/")) {
      IntraPredFilterIntraInit_NEON();
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }

    // Put the current architecture-specific implementation up for testing and
    // comparison against C version.
    cur_filter_intra_pred_ = dsp->filter_intra_predictor;
  }

  // These tests modify intra_pred_mem_.
  void TestSpeed(const char* const digests[kNumFilterIntraPredictors],
                 int num_runs);
  void TestSaturatedValues();
  void TestRandomValues();

  FilterIntraPredictorFunc base_filter_intra_pred_;
  FilterIntraPredictorFunc cur_filter_intra_pred_;
};

template <int bitdepth, typename Pixel>
void FilterIntraPredTest<bitdepth, Pixel>::TestSpeed(
    const char* const digests[kNumFilterIntraPredictors], const int num_runs) {
  ASSERT_NE(digests, nullptr);
  const Pixel* const left = intra_pred_mem_.left_mem + 16;
  const Pixel* const top = intra_pred_mem_.top_mem + 16;

  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  intra_pred_mem_.Reset(&rnd);

  // IntraPredInit_C() leaves the filter function empty.
  if (cur_filter_intra_pred_ == nullptr) return;
  for (int i = 0; i < kNumFilterIntraPredictors; ++i) {
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const absl::Time start = absl::Now();
    for (int run = 0; run < num_runs; ++run) {
      const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
      cur_filter_intra_pred_(intra_pred_mem_.dst, stride, top, left,
                             static_cast<FilterIntraPredictor>(i), block_width_,
                             block_height_);
    }
    const absl::Duration elapsed_time = absl::Now() - start;
    test_utils::CheckMd5Digest(
        kTransformSizeNames[tx_size_], kFilterIntraPredNames[i], digests[i],
        intra_pred_mem_.dst, sizeof(intra_pred_mem_.dst), elapsed_time);
  }
}

template <int bitdepth, typename Pixel>
void FilterIntraPredTest<bitdepth, Pixel>::TestSaturatedValues() {
  Pixel* const left = intra_pred_mem_.left_mem + 16;
  Pixel* const top = intra_pred_mem_.top_mem + 16;
  const auto kMaxPixel = static_cast<Pixel>((1 << bitdepth) - 1);
  intra_pred_mem_.Set(kMaxPixel);

  // IntraPredInit_C() leaves the filter function empty.
  if (cur_filter_intra_pred_ == nullptr) return;
  for (int i = 0; i < kNumFilterIntraPredictors; ++i) {
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
    cur_filter_intra_pred_(intra_pred_mem_.dst, stride, top, left,
                           static_cast<FilterIntraPredictor>(i), block_width_,
                           block_height_);
    if (!test_utils::CompareBlocks(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
                                   block_width_, block_height_, kMaxBlockSize,
                                   kMaxBlockSize, true)) {
      ADD_FAILURE() << "Expected " << kFilterIntraPredNames[i]
                    << " to produce a block containing '"
                    << static_cast<int>(kMaxPixel) << "'";
    }
  }
}

template <int bitdepth, typename Pixel>
void FilterIntraPredTest<bitdepth, Pixel>::TestRandomValues() {
  // Skip the 'C' test case as this is used as the reference.
  if (base_filter_intra_pred_ == nullptr) return;

  // Use an alternate seed to differentiate this test from TestSpeed().
  libvpx_test::ACMRandom rnd(test_utils::kAlternateDeterministicSeed);
  for (int i = 0; i < kNumFilterIntraPredictors; ++i) {
    // It may be worthwhile to temporarily increase this loop size when testing
    // changes that specifically affect this test.
    for (int n = 0; n < 10000; ++n) {
      intra_pred_mem_.Reset(&rnd);

      memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
             sizeof(intra_pred_mem_.dst));
      const Pixel* const top = intra_pred_mem_.top_mem + 16;
      const Pixel* const left = intra_pred_mem_.left_mem + 16;
      const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
      base_filter_intra_pred_(intra_pred_mem_.ref_src, stride, top, left,
                              static_cast<FilterIntraPredictor>(i),
                              block_width_, block_height_);
      cur_filter_intra_pred_(intra_pred_mem_.dst, stride, top, left,
                             static_cast<FilterIntraPredictor>(i), block_width_,
                             block_height_);
      if (!test_utils::CompareBlocks(
              intra_pred_mem_.dst, intra_pred_mem_.ref_src, block_width_,
              block_height_, kMaxBlockSize, kMaxBlockSize, true)) {
        ADD_FAILURE() << "Result from optimized version of "
                      << kFilterIntraPredNames[i]
                      << " differs from reference in iteration #" << n;
        break;
      }
    }
  }
}

//------------------------------------------------------------------------------
// CflIntraPredTest

template <int bitdepth, typename Pixel>
class CflIntraPredTest : public IntraPredTestBase<bitdepth, Pixel> {
 public:
  CflIntraPredTest() = default;
  CflIntraPredTest(const CflIntraPredTest&) = delete;
  CflIntraPredTest& operator=(const CflIntraPredTest&) = delete;
  ~CflIntraPredTest() override = default;

 protected:
  using IntraPredTestBase<bitdepth, Pixel>::tx_size_;
  using IntraPredTestBase<bitdepth, Pixel>::block_width_;
  using IntraPredTestBase<bitdepth, Pixel>::block_height_;
  using IntraPredTestBase<bitdepth, Pixel>::intra_pred_mem_;

  void SetUp() override {
    IntraPredTestBase<bitdepth, Pixel>::SetUp();
    IntraPredInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_cfl_intra_pred_ = dsp->cfl_intra_predictors[tx_size_];

    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      base_cfl_intra_pred_ = nullptr;
    } else if (absl::StartsWith(test_case, "NEON/")) {
      IntraPredCflInit_NEON();
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        IntraPredCflInit_SSE4_1();
      }
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }

    cur_cfl_intra_pred_ = dsp->cfl_intra_predictors[tx_size_];

    if (cur_cfl_intra_pred_ == base_cfl_intra_pred_) {
      cur_cfl_intra_pred_ = nullptr;
    }
  }

  // This test modifies intra_pred_mem_.
  void TestSpeed(const char* digest, int num_runs);
  void TestSaturatedValues();
  void TestRandomValues();

  CflIntraPredictorFunc base_cfl_intra_pred_;
  CflIntraPredictorFunc cur_cfl_intra_pred_;
};

template <int bitdepth, typename Pixel>
void CflIntraPredTest<bitdepth, Pixel>::TestSpeed(const char* const digest,
                                                  const int num_runs) {
  if (cur_cfl_intra_pred_ == nullptr) return;
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride] = {};
  const int alpha = rnd(33) - 16;
  const int dc = rnd(1 << bitdepth);
  const int max_luma = ((1 << bitdepth) - 1) << 3;
  for (int i = 0; i < block_height_; ++i) {
    for (int j = 0; j < block_width_; ++j) {
      if (i < kCflLumaBufferStride && j < kCflLumaBufferStride) {
        luma[i][j] = max_luma - rnd(max_luma << 1);
      }
    }
  }
  for (auto& r : intra_pred_mem_.ref_src) r = dc;

  absl::Duration elapsed_time;
  for (int run = 0; run < num_runs; ++run) {
    const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const absl::Time start = absl::Now();
    cur_cfl_intra_pred_(intra_pred_mem_.dst, stride, luma, alpha);
    elapsed_time += absl::Now() - start;
  }
  test_utils::CheckMd5Digest(kTransformSizeNames[tx_size_], kCflIntraPredName,
                             digest, intra_pred_mem_.dst,
                             sizeof(intra_pred_mem_.dst), elapsed_time);
}

template <int bitdepth, typename Pixel>
void CflIntraPredTest<bitdepth, Pixel>::TestSaturatedValues() {
  // Skip the 'C' test case as this is used as the reference.
  if (base_cfl_intra_pred_ == nullptr) return;

  int16_t luma_buffer[kCflLumaBufferStride][kCflLumaBufferStride];
  for (auto& line : luma_buffer) {
    for (auto& luma : line) luma = ((1 << bitdepth) - 1) << 3;
  }

  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  static constexpr int kSaturatedAlpha[] = {-16, 16};
  for (const int alpha : kSaturatedAlpha) {
    for (auto& r : intra_pred_mem_.ref_src) r = (1 << bitdepth) - 1;
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
    base_cfl_intra_pred_(intra_pred_mem_.ref_src, stride, luma_buffer, alpha);
    cur_cfl_intra_pred_(intra_pred_mem_.dst, stride, luma_buffer, alpha);
    if (!test_utils::CompareBlocks(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
                                   block_width_, block_height_, kMaxBlockSize,
                                   kMaxBlockSize, true)) {
      ADD_FAILURE() << "Result from optimized version of CFL with alpha "
                    << alpha << " differs from reference.";
      break;
    }
  }
}

template <int bitdepth, typename Pixel>
void CflIntraPredTest<bitdepth, Pixel>::TestRandomValues() {
  // Skip the 'C' test case as this is used as the reference.
  if (base_cfl_intra_pred_ == nullptr) return;
  int16_t luma_buffer[kCflLumaBufferStride][kCflLumaBufferStride];

  const int max_luma = ((1 << bitdepth) - 1) << 3;
  // Use an alternate seed to differentiate this test from TestSpeed().
  libvpx_test::ACMRandom rnd(test_utils::kAlternateDeterministicSeed);
  for (auto& line : luma_buffer) {
    for (auto& luma : line) luma = max_luma - rnd(max_luma << 1);
  }
  const int dc = rnd(1 << bitdepth);
  for (auto& r : intra_pred_mem_.ref_src) r = dc;
  static constexpr int kSaturatedAlpha[] = {-16, 16};
  for (const int alpha : kSaturatedAlpha) {
    intra_pred_mem_.Reset(&rnd);
    memcpy(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
           sizeof(intra_pred_mem_.dst));
    const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
    base_cfl_intra_pred_(intra_pred_mem_.ref_src, stride, luma_buffer, alpha);
    cur_cfl_intra_pred_(intra_pred_mem_.dst, stride, luma_buffer, alpha);
    if (!test_utils::CompareBlocks(intra_pred_mem_.dst, intra_pred_mem_.ref_src,
                                   block_width_, block_height_, kMaxBlockSize,
                                   kMaxBlockSize, true)) {
      ADD_FAILURE() << "Result from optimized version of CFL with alpha "
                    << alpha << " differs from reference.";
      break;
    }
  }
}

template <int bitdepth, typename Pixel, SubsamplingType subsampling_type>
class CflSubsamplerTest : public IntraPredTestBase<bitdepth, Pixel> {
 public:
  CflSubsamplerTest() = default;
  CflSubsamplerTest(const CflSubsamplerTest&) = delete;
  CflSubsamplerTest& operator=(const CflSubsamplerTest&) = delete;
  ~CflSubsamplerTest() override = default;

 protected:
  using IntraPredTestBase<bitdepth, Pixel>::tx_size_;
  using IntraPredTestBase<bitdepth, Pixel>::block_width_;
  using IntraPredTestBase<bitdepth, Pixel>::block_height_;
  using IntraPredTestBase<bitdepth, Pixel>::intra_pred_mem_;

  void SetUp() override {
    IntraPredTestBase<bitdepth, Pixel>::SetUp();
    IntraPredInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_cfl_subsampler_ = dsp->cfl_subsamplers[tx_size_][subsampling_type];

    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      base_cfl_subsampler_ = nullptr;
    } else if (absl::StartsWith(test_case, "NEON/")) {
      IntraPredCflInit_NEON();
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        IntraPredCflInit_SSE4_1();
      }
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
    cur_cfl_subsampler_ = dsp->cfl_subsamplers[tx_size_][subsampling_type];
  }

  // This test modifies intra_pred_mem_.
  void TestSpeed(const char* digest, int num_runs);
  void TestSaturatedValues();
  void TestRandomValues();

  enum SubsamplingType SubsamplingType() const { return subsampling_type; }

  CflSubsamplerFunc base_cfl_subsampler_;
  CflSubsamplerFunc cur_cfl_subsampler_;
};

// There is no case where both source and output have lowest height or width
// when that dimension is subsampled.
int GetLumaWidth(int block_width, SubsamplingType subsampling_type) {
  if (block_width == 4) {
    const int width_shift =
        static_cast<int>(subsampling_type != kSubsamplingType444);
    return block_width << width_shift;
  }
  return block_width;
}

int GetLumaHeight(int block_height, SubsamplingType subsampling_type) {
  if (block_height == 4) {
    const int height_shift =
        static_cast<int>(subsampling_type == kSubsamplingType420);
    return block_height << height_shift;
  }
  return block_height;
}

template <int bitdepth, typename Pixel, SubsamplingType subsampling_type>
void CflSubsamplerTest<bitdepth, Pixel, subsampling_type>::TestSpeed(
    const char* const digest, const int num_runs) {
  // C declines initializing the table in normal circumstances because there are
  // assembly implementations.
  if (cur_cfl_subsampler_ == nullptr) return;
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());

  const int width = GetLumaWidth(block_width_, subsampling_type);
  const int height = GetLumaHeight(block_height_, subsampling_type);
  Pixel* src = intra_pred_mem_.ref_src;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      src[j] = rnd.RandRange(1 << bitdepth);
    }
    src += kMaxBlockSize;
  }
  const absl::Time start = absl::Now();
  int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride] = {};
  const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
  for (int run = 0; run < num_runs; ++run) {
    cur_cfl_subsampler_(luma, width, height, intra_pred_mem_.ref_src, stride);
  }
  const absl::Duration elapsed_time = absl::Now() - start;
  test_utils::CheckMd5Digest(kTransformSizeNames[tx_size_], kCflIntraPredName,
                             digest, luma, sizeof(luma), elapsed_time);
}

template <int bitdepth, typename Pixel, SubsamplingType subsampling_type>
void CflSubsamplerTest<bitdepth, Pixel,
                       subsampling_type>::TestSaturatedValues() {
  if (base_cfl_subsampler_ == nullptr) return;
  const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
  for (int width = GetLumaWidth(block_width_, subsampling_type); width > 0;
       width -= 8) {
    for (int height = GetLumaHeight(block_height_, subsampling_type);
         height > 0; height -= 8) {
      Pixel* src = intra_pred_mem_.ref_src;
      for (int y = 0; y < height; ++y) {
        Memset(src, (1 << bitdepth) - 1, width);
        Memset(src + width, 0, kMaxBlockSize - width);
        src += kMaxBlockSize;
      }
      Memset(intra_pred_mem_.ref_src + kMaxBlockSize * height, 0,
             kMaxBlockSize * (kMaxBlockSize - height));

      int16_t luma_base[kCflLumaBufferStride][kCflLumaBufferStride] = {};
      int16_t luma_cur[kCflLumaBufferStride][kCflLumaBufferStride] = {};
      base_cfl_subsampler_(luma_base, width, height, intra_pred_mem_.ref_src,
                           stride);
      cur_cfl_subsampler_(luma_cur, width, height, intra_pred_mem_.ref_src,
                          stride);
      if (!test_utils::CompareBlocks(reinterpret_cast<uint16_t*>(luma_cur[0]),
                                     reinterpret_cast<uint16_t*>(luma_base[0]),
                                     block_width_, block_height_,
                                     kCflLumaBufferStride, kCflLumaBufferStride,
                                     true)) {
        FAIL() << "Result from optimized version of CFL subsampler"
               << " differs from reference. max_luma_width: " << width
               << " max_luma_height: " << height;
      }
    }
  }
}

template <int bitdepth, typename Pixel, SubsamplingType subsampling_type>
void CflSubsamplerTest<bitdepth, Pixel, subsampling_type>::TestRandomValues() {
  if (base_cfl_subsampler_ == nullptr) return;
  const ptrdiff_t stride = kMaxBlockSize * sizeof(Pixel);
  // Use an alternate seed to differentiate this test from TestSpeed().
  libvpx_test::ACMRandom rnd(test_utils::kAlternateDeterministicSeed);
  for (int width = GetLumaWidth(block_width_, subsampling_type); width > 0;
       width -= 8) {
    for (int height = GetLumaHeight(block_height_, subsampling_type);
         height > 0; height -= 8) {
      Pixel* src = intra_pred_mem_.ref_src;
      for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
          src[j] = rnd.RandRange(1 << bitdepth);
        }
        Memset(src + width, 0, kMaxBlockSize - width);
        src += kMaxBlockSize;
      }
      Memset(intra_pred_mem_.ref_src + kMaxBlockSize * height, 0,
             kMaxBlockSize * (kMaxBlockSize - height));

      int16_t luma_base[kCflLumaBufferStride][kCflLumaBufferStride] = {};
      int16_t luma_cur[kCflLumaBufferStride][kCflLumaBufferStride] = {};
      base_cfl_subsampler_(luma_base, width, height, intra_pred_mem_.ref_src,
                           stride);
      cur_cfl_subsampler_(luma_cur, width, height, intra_pred_mem_.ref_src,
                          stride);
      if (!test_utils::CompareBlocks(reinterpret_cast<uint16_t*>(luma_cur[0]),
                                     reinterpret_cast<uint16_t*>(luma_base[0]),
                                     block_width_, block_height_,
                                     kCflLumaBufferStride, kCflLumaBufferStride,
                                     true)) {
        FAIL() << "Result from optimized version of CFL subsampler"
               << " differs from reference. max_luma_width: " << width
               << " max_luma_height: " << height;
      }
    }
  }
}

//------------------------------------------------------------------------------

using IntraPredTest8bpp = IntraPredTest<8, uint8_t>;

const char* const* GetIntraPredDigests8bpp(TransformSize tx_size) {
  static const char* const kDigests4x4[kNumIntraPredictors] = {
      "7b1c762e28747f885d2b7d83cb8aa75c", "73353f179207f1432d40a132809e3a50",
      "80c9237c838b0ec0674ccb070df633d5", "1cd79116b41fda884e7fa047f5eb14df",
      "33211425772ee539a59981a2e9dc10c1", "d6f5f65a267f0e9a2752e8151cc1dcd7",
      "7ff8c762cb766eb0665682152102ce4b", "2276b861ae4599de15938651961907ec",
      "766982bc69f4aaaa8e71014c2dc219bc", "e2c31b5fd2199c49e17c31610339ab3f",
  };
  static const char* const kDigests4x8[kNumIntraPredictors] = {
      "0a0d8641ecfa0e82f541acdc894d5574", "1a40371af6cff9c278c5b0def9e4b3e7",
      "3631a7a99569663b514f15b590523822", "646c7b592136285bd31501494e7393e7",
      "ecbe89cc64dc2688123d3cfe865b5237", "79048e70ecbb7d43a4703f62718588c0",
      "f3de11bf1198a00675d806d29c41d676", "32bb6cd018f6e871c342fcc21c7180cf",
      "6f076a1e5ab3d69cf08811d62293e4be", "2a84460a8b189b4589824cf6b3b39954",
  };
  static const char* const kDigests4x16[kNumIntraPredictors] = {
      "cb8240be98444ede5ae98ca94afc1557", "460acbcf825a1fa0d8f2aa6bf2d6a21c",
      "7896fdbbfe538dce1dc3a5b0873d74b0", "504aea29c6b27f21555d5516b8de2d8a",
      "c5738e7fa82b91ea0e39232120da56ea", "19abbd934c243a6d9df7585d81332dd5",
      "9e42b7b342e45c842dfa8aedaddbdfaa", "0e9eb07a89f8bf96bc219d5d1c3d9f6d",
      "659393c31633e0f498bae384c9df5c7b", "bee3a28312da99dd550ec309ae4fff25",
  };
  static const char* const kDigests8x4[kNumIntraPredictors] = {
      "5950744064518f77867c8e14ebd8b5d7", "46b6cbdc76efd03f4ac77870d54739f7",
      "efe21fd1b98cb1663950e0bf49483b3b", "3c647b64760b298092cbb8e2f5c06bfd",
      "c3595929687ffb04c59b128d56e2632f", "d89ad2ddf8a74a520fdd1d7019fd75b4",
      "53907cb70ad597ee5885f6c58201f98b", "09d2282a29008b7fb47eb60ed6653d06",
      "e341fc1c910d7cb2dac5dbc58b9c9af9", "a8fabd4c259b607a90a2e4d18cae49de",
  };
  static const char* const kDigests8x8[kNumIntraPredictors] = {
      "06fb7cb52719855a38b4883b4b241749", "2013aafd42a4303efb553e42264ab8b0",
      "2f070511d5680c12ca73a20e47fd6e23", "9923705af63e454392625794d5459fe0",
      "04007a0d39778621266e2208a22c4fac", "2d296c202d36b4a53f1eaddda274e4a1",
      "c87806c220d125c7563c2928e836fbbd", "339b49710a0099087e51ab5afc8d8713",
      "c90fbc020afd9327bf35dccae099bf77", "95b356a7c346334d29294a5e2d13cfd9",
  };
  static const char* const kDigests8x16[kNumIntraPredictors] = {
      "3c5a4574d96b5bb1013429636554e761", "8cf56b17c52d25eb785685f2ab48b194",
      "7911e2e02abfbe226f17529ac5db08fc", "064e509948982f66a14293f406d88d42",
      "5c443aa713891406d5be3af4b3cf67c6", "5d2cb98e532822ca701110cda9ada968",
      "3d58836e17918b8890012dd96b95bb9d", "20e8d61ddc451b9e553a294073349ffd",
      "a9aa6cf9d0dcf1977a1853ccc264e40b", "103859f85750153f47b81f68ab7881f2",
  };
  static const char* const kDigests8x32[kNumIntraPredictors] = {
      "b393a2db7a76acaccc39e04d9dc3e8ac", "bbda713ee075a7ef095f0f479b5a1f82",
      "f337dce3980f70730d6f6c2c756e3b62", "796189b05dc026e865c9e95491b255d1",
      "ea932c21e7189eeb215c1990491320ab", "a9fffdf9455eba5e3b01317cae140289",
      "9525dbfdbf5fba61ef9c7aa5fe887503", "8c6a7e3717ff8a459f415c79bb17341c",
      "3761071bfaa2363a315fe07223f95a2d", "0e5aeb9b3f485b90df750469f60c15aa",
  };
  static const char* const kDigests16x4[kNumIntraPredictors] = {
      "1c0a950b3ac500def73b165b6a38467c", "95e7f7300f19da280c6a506e40304462",
      "28a6af15e31f76d3ff189012475d78f5", "e330d67b859bceef62b96fc9e1f49a34",
      "36eca3b8083ce2fb5f7e6227dfc34e71", "08f567d2abaa8e83e4d9b33b3f709538",
      "dc2d0ba13aa9369446932f03b53dc77d", "9ab342944c4b1357aa79d39d7bebdd3a",
      "77ec278c5086c88b91d68eef561ed517", "60fbe11bfe216c182aaacdec326c4dae",
  };
  static const char* const kDigests16x8[kNumIntraPredictors] = {
      "053a2bc4b5b7287fee524af4e77f077a", "619b720b13f14f32391a99ea7ff550d5",
      "728d61c11b06baf7fe77881003a918b9", "889997b89a44c9976cb34f573e2b1eea",
      "b43bfc31d1c770bb9ca5ca158c9beec4", "9d3fe9f762e0c6e4f114042147c50c7f",
      "c74fdd7c9938603b01e7ecf9fdf08d61", "870c7336db1102f80f74526bd5a7cf4e",
      "3fd5354a6190903d6a0b661fe177daf6", "409ca6b0b2558aeadf5ef2b8a887e67a",
  };
  static const char* const kDigests16x16[kNumIntraPredictors] = {
      "1fa9e2086f6594bda60c30384fbf1635", "2098d2a030cd7c6be613edc74dc2faf8",
      "f3c72b0c8e73f1ddca04d14f52d194d8", "6b31f2ee24cf88d3844a2fc67e1f39f3",
      "d91a22a83575e9359c5e4871ab30ddca", "24c32a0d38b4413d2ef9bf1f842c8634",
      "6e9e47bf9da9b2b9ae293e0bbd8ff086", "968b82804b5200b074bcdba9718140d4",
      "4e6d7e612c5ae0bbdcc51a453cd1db3f", "ce763a41977647d072f33e277d69c7b9",
  };
  static const char* const kDigests16x32[kNumIntraPredictors] = {
      "01afd04432026ff56327d6226b720be2", "a6e7be906cc6f1e7a520151bfa7c303d",
      "bc05c46f18d0638f0228f1de64f07cd5", "204e613e429935f721a5b29cec7d44bb",
      "aa0a7c9a7482dfc06d9685072fc5bafd", "ffb60f090d83c624bb4f7dc3a630ac4f",
      "36bcb9ca9bb5eac520b050409de25da5", "34d9a5dd3363668391bc3bd05b468182",
      "1e149c28db8b234e43931c347a523794", "6e8aff02470f177c3ff4416db79fc508",
  };
  static const char* const kDigests16x64[kNumIntraPredictors] = {
      "727797ef15ccd8d325476fe8f12006a3", "f77c544ac8035e01920deae40cee7b07",
      "12b0c69595328c465e0b25e0c9e3e9fc", "3b2a053ee8b05a8ac35ad23b0422a151",
      "f3be77c0fe67eb5d9d515e92bec21eb7", "f1ece6409e01e9dd98b800d49628247d",
      "efd2ec9bfbbd4fd1f6604ea369df1894", "ec703de918422b9e03197ba0ed60a199",
      "739418efb89c07f700895deaa5d0b3e3", "9943ae1bbeeebfe1d3a92dc39e049d63",
  };
  static const char* const kDigests32x8[kNumIntraPredictors] = {
      "4da55401331ed98acec0c516d7307513", "0ae6f3974701a5e6c20baccd26b4ca52",
      "79b799f1eb77d5189535dc4e18873a0e", "90e943adf3de4f913864dce4e52b4894",
      "5e1b9cc800a89ef45f5bdcc9e99e4e96", "3103405df20d254cbf32ac30872ead4b",
      "648550e369b77687bff3c7d6f249b02f", "f9f73bcd8aadfc059fa260325df957a1",
      "204cef70d741c25d4fe2b1d10d2649a5", "04c05e18488496eba64100faa25e8baf",
  };
  static const char* const kDigests32x16[kNumIntraPredictors] = {
      "86ad1e1047abaf9959150222e8f19593", "1908cbe04eb4e5c9d35f1af7ffd7ee72",
      "6ad3bb37ebe8374b0a4c2d18fe3ebb6a", "08d3cfe7a1148bff55eb6166da3378c6",
      "656a722394764d17b6c42401b9e0ad3b", "4aa00c192102efeb325883737e562f0d",
      "9881a90ca88bca4297073e60b3bb771a", "8cd74aada398a3d770fc3ace38ecd311",
      "0a927e3f5ff8e8338984172cc0653b13", "d881d68b4eb3ee844e35e04ad6721f5f",
  };
  static const char* const kDigests32x32[kNumIntraPredictors] = {
      "1303ca680644e3d8c9ffd4185bb2835b", "2a4d9f5cc8da307d4cf7dc021df10ba9",
      "ced60d3f4e4b011a6a0314dd8a4b1fd8", "ced60d3f4e4b011a6a0314dd8a4b1fd8",
      "1464b01aa928e9bd82c66bad0f921693", "90deadfb13d7c3b855ba21b326c1e202",
      "af96a74f8033dff010e53a8521bc6f63", "9f1039f2ef082aaee69fcb7d749037c2",
      "3f82893e478e204f2d254b34222d14dc", "ddb2b95ffb65b84dd4ff1f7256223305",
  };
  static const char* const kDigests32x64[kNumIntraPredictors] = {
      "e1e8ed803236367821981500a3d9eebe", "0f46d124ba9f48cdd5d5290acf786d6d",
      "4e2a2cfd8f56f15939bdfc753145b303", "0ce332b343934b34cd4417725faa85cb",
      "1d2f8e48e3adb7c448be05d9f66f4954", "9fb2e176636a5689b26f73ca73fcc512",
      "e720ebccae7e25e36f23da53ae5b5d6a", "86fe4364734169aaa4520d799890d530",
      "b1870290764bb1b100d1974e2bd70f1d", "ce5b238e19d85ef69d85badfab4e63ae",
  };
  static const char* const kDigests64x16[kNumIntraPredictors] = {
      "de1b736e9d99129609d6ef3a491507a0", "516d8f6eb054d74d150e7b444185b6b9",
      "69e462c3338a9aaf993c3f7cfbc15649", "821b76b1494d4f84d20817840f719a1a",
      "fd9b4276e7affe1e0e4ce4f428058994", "cd82fd361a4767ac29a9f406b480b8f3",
      "2792c2f810157a4a6cb13c28529ff779", "1220442d90c4255ba0969d28b91e93a6",
      "c7253e10b45f7f67dfee3256c9b94825", "879792198071c7e0b50b9b5010d8c18f",
  };
  static const char* const kDigests64x32[kNumIntraPredictors] = {
      "e48e1ac15e97191a8fda08d62fff343e", "80c15b303235f9bc2259027bb92dfdc4",
      "538424b24bd0830f21788e7238ca762f", "a6c5aeb722615089efbca80b02951ceb",
      "12604b37875533665078405ef4582e35", "0048afa17bd3e1632d68b96048836530",
      "07a0cfcb56a5eed50c4bd6c26814336b", "529d8a070de5bc6531fa3ee8f450c233",
      "33c50a11c7d78f72434064f634305e95", "e0ef7f0559c1a50ec5a8c12011b962f7",
  };
  static const char* const kDigests64x64[kNumIntraPredictors] = {
      "a1650dbcd56e10288c3e269eca37967d", "be91585259bc37bf4dc1651936e90b3e",
      "afe020786b83b793c2bbd9468097ff6e", "6e1094fa7b50bc813aa2ba29f5df8755",
      "9e5c34f3797e0cdd3cd9d4c05b0d8950", "bc87be7ac899cc6a28f399d7516c49fe",
      "9811fd0d2dd515f06122f5d1bd18b784", "3c140e466f2c2c0d9cb7d2157ab8dc27",
      "9543de76c925a8f6adc884cc7f98dc91", "df1df0376cc944afe7e74e94f53e575a",
  };

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4;
    case kTransformSize4x8:
      return kDigests4x8;
    case kTransformSize4x16:
      return kDigests4x16;
    case kTransformSize8x4:
      return kDigests8x4;
    case kTransformSize8x8:
      return kDigests8x8;
    case kTransformSize8x16:
      return kDigests8x16;
    case kTransformSize8x32:
      return kDigests8x32;
    case kTransformSize16x4:
      return kDigests16x4;
    case kTransformSize16x8:
      return kDigests16x8;
    case kTransformSize16x16:
      return kDigests16x16;
    case kTransformSize16x32:
      return kDigests16x32;
    case kTransformSize16x64:
      return kDigests16x64;
    case kTransformSize32x8:
      return kDigests32x8;
    case kTransformSize32x16:
      return kDigests32x16;
    case kTransformSize32x32:
      return kDigests32x32;
    case kTransformSize32x64:
      return kDigests32x64;
    case kTransformSize64x16:
      return kDigests64x16;
    case kTransformSize64x32:
      return kDigests64x32;
    case kTransformSize64x64:
      return kDigests64x64;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(IntraPredTest8bpp, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetIntraPredDigests8bpp(tx_size_), num_runs);
}

TEST_P(IntraPredTest8bpp, FixedInput) {
  TestSpeed(GetIntraPredDigests8bpp(tx_size_), 1);
}

TEST_P(IntraPredTest8bpp, Overflow) { TestSaturatedValues(); }
TEST_P(IntraPredTest8bpp, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

using DirectionalIntraPredTest8bpp = DirectionalIntraPredTest<8, uint8_t>;

const char* const* GetDirectionalIntraPredDigests8bpp(TransformSize tx_size) {
  static const char* const kDigests4x4[kNumDirectionalIntraPredictors] = {
      "9cfc1da729ad08682e165826c29b280b",
      "bb73539c7afbda7bddd2184723b932d6",
      "9d2882800ffe948196e984a26a2da72c",
  };
  static const char* const kDigests4x8[kNumDirectionalIntraPredictors] = {
      "090efe6f83cc6fa301f65d3bbd5c38d2",
      "d0fba4cdfb90f8bd293a94cae9db1a15",
      "f7ad0eeab4389d0baa485d30fec87617",
  };
  static const char* const kDigests4x16[kNumDirectionalIntraPredictors] = {
      "1d32b33c75fe85248c48cdc8caa78d84",
      "7000e18159443d366129a6cc6ef8fcee",
      "06c02fac5f8575f687abb3f634eb0b4c",
  };
  static const char* const kDigests8x4[kNumDirectionalIntraPredictors] = {
      "1b591799685bc135982114b731293f78",
      "5cd9099acb9f7b2618dafa6712666580",
      "d023883efede88f99c19d006044d9fa1",
  };
  static const char* const kDigests8x8[kNumDirectionalIntraPredictors] = {
      "f1e46ecf62a2516852f30c5025adb7ea",
      "864442a209c16998065af28d8cdd839a",
      "411a6e554868982af577de69e53f12e8",
  };
  static const char* const kDigests8x16[kNumDirectionalIntraPredictors] = {
      "89278302be913a85cfb06feaea339459",
      "6c42f1a9493490cd4529fd40729cec3c",
      "2516b5e1c681e5dcb1acedd5f3d41106",
  };
  static const char* const kDigests8x32[kNumDirectionalIntraPredictors] = {
      "aea7078f3eeaa8afbfe6c959c9e676f1",
      "cad30babf12729dda5010362223ba65c",
      "ff384ebdc832007775af418a2aae1463",
  };
  static const char* const kDigests16x4[kNumDirectionalIntraPredictors] = {
      "964a821c313c831e12f4d32e616c0b55",
      "adf6dad3a84ab4d16c16eea218bec57a",
      "a54fa008d43895e523474686c48a81c2",
  };
  static const char* const kDigests16x8[kNumDirectionalIntraPredictors] = {
      "fe2851b4e4f9fcf924cf17d50415a4c0",
      "50a0e279c481437ff315d08eb904c733",
      "0682065c8fb6cbf9be4949316c87c9e5",
  };
  static const char* const kDigests16x16[kNumDirectionalIntraPredictors] = {
      "ef15503b1943642e7a0bace1616c0e11",
      "bf1a4d3f855f1072a902a88ec6ce0350",
      "7e87a03e29cd7fd843fd71b729a18f3f",
  };
  static const char* const kDigests16x32[kNumDirectionalIntraPredictors] = {
      "f7b636615d2e5bf289b5db452a6f188d",
      "e95858c532c10d00b0ce7a02a02121dd",
      "34a18ccf58ef490f32268e85ce8c7de4",
  };
  static const char* const kDigests16x64[kNumDirectionalIntraPredictors] = {
      "b250099986c2fab9670748598058846b",
      "f25d80af4da862a9b6b72979f1e17cb4",
      "5347dc7bc346733b4887f6c8ad5e0898",
  };
  static const char* const kDigests32x8[kNumDirectionalIntraPredictors] = {
      "72e4c9f8af043b1cb1263490351818ab",
      "1fc010d2df011b9e4e3d0957107c78df",
      "f4cbfa3ca941ef08b972a68d7e7bafc4",
  };
  static const char* const kDigests32x16[kNumDirectionalIntraPredictors] = {
      "37e5a1aaf7549d2bce08eece9d20f0f6",
      "6a2794025d0aca414ab17baa3cf8251a",
      "63dd37a6efdc91eeefef166c99ce2db1",
  };
  static const char* const kDigests32x32[kNumDirectionalIntraPredictors] = {
      "198aabc958992eb49cceab97d1acb43e",
      "aee88b6c8bacfcf38799fe338e6c66e7",
      "01e8f8f96696636f6d79d33951907a16",
  };
  static const char* const kDigests32x64[kNumDirectionalIntraPredictors] = {
      "0611390202c4f90f7add7aec763ded58",
      "960240c7ceda2ccfac7c90b71460578a",
      "7e7d97594aab8ad56e8c01c340335607",
  };
  static const char* const kDigests64x16[kNumDirectionalIntraPredictors] = {
      "7e1f567e7fc510757f2d89d638bc826f",
      "c929d687352ce40a58670be2ce3c8c90",
      "f6881e6a9ba3c3d3d730b425732656b1",
  };
  static const char* const kDigests64x32[kNumDirectionalIntraPredictors] = {
      "27b4c2a7081d4139f22003ba8b6dfdf2",
      "301e82740866b9274108a04c872fa848",
      "98d3aa4fef838f4abf00dac33806659f",
  };
  static const char* const kDigests64x64[kNumDirectionalIntraPredictors] = {
      "b31816db8fade3accfd975b21aa264c7",
      "2adce01a03b9452633d5830e1a9b4e23",
      "7b988fadba8b07c36e88d7be6b270494",
  };

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4;
    case kTransformSize4x8:
      return kDigests4x8;
    case kTransformSize4x16:
      return kDigests4x16;
    case kTransformSize8x4:
      return kDigests8x4;
    case kTransformSize8x8:
      return kDigests8x8;
    case kTransformSize8x16:
      return kDigests8x16;
    case kTransformSize8x32:
      return kDigests8x32;
    case kTransformSize16x4:
      return kDigests16x4;
    case kTransformSize16x8:
      return kDigests16x8;
    case kTransformSize16x16:
      return kDigests16x16;
    case kTransformSize16x32:
      return kDigests16x32;
    case kTransformSize16x64:
      return kDigests16x64;
    case kTransformSize32x8:
      return kDigests32x8;
    case kTransformSize32x16:
      return kDigests32x16;
    case kTransformSize32x32:
      return kDigests32x32;
    case kTransformSize32x64:
      return kDigests32x64;
    case kTransformSize64x16:
      return kDigests64x16;
    case kTransformSize64x32:
      return kDigests64x32;
    case kTransformSize64x64:
      return kDigests64x64;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(DirectionalIntraPredTest8bpp, DISABLED_Speed) {
  const auto num_runs = static_cast<int>(5e7 / (block_width_ * block_height_));
  for (int i = kZone1; i < kNumZones; ++i) {
    TestSpeed(GetDirectionalIntraPredDigests8bpp(tx_size_),
              static_cast<Zone>(i), num_runs);
  }
}

TEST_P(DirectionalIntraPredTest8bpp, FixedInput) {
  for (int i = kZone1; i < kNumZones; ++i) {
    TestSpeed(GetDirectionalIntraPredDigests8bpp(tx_size_),
              static_cast<Zone>(i), 1);
  }
}

TEST_P(DirectionalIntraPredTest8bpp, Overflow) { TestSaturatedValues(); }
TEST_P(DirectionalIntraPredTest8bpp, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

using FilterIntraPredTest8bpp = FilterIntraPredTest<8, uint8_t>;

const char* const* GetFilterIntraPredDigests8bpp(TransformSize tx_size) {
  static const char* const kDigests4x4[kNumFilterIntraPredictors] = {
      "a2486efcfb351d60a8941203073e89c6", "240716ae5ecaedc19edae1bdef49e05d",
      "dacf4af66a966aca7c75abe24cd9ba99", "311888773676f3c2ae3334c4e0f141e5",
      "2d3711616c8d8798f608e313cb07a72a",
  };
  static const char* const kDigests4x8[kNumFilterIntraPredictors] = {
      "1cb74ba1abc68d936e87c13511ed5fbf", "d64c2c08586a762dbdfa8e1150bede06",
      "73e9d1a9b6fa3e96fbd65c7dce507529", "e3ae17d9338e5aa3420d31d0e2d7ee87",
      "750dbfe3bc5508b7031957a1d315b8bc",
  };
  static const char* const kDigests4x16[kNumFilterIntraPredictors] = {
      "48a1060701bf68ec6342d6e24c10ef17", "0c91ff7988814d192ed95e840a87b4bf",
      "efe586b891c8828c4116c9fbf50850cc", "a3bfa10be2b155826f107e9256ac3ba1",
      "976273745b94a561fd52f5aa96fb280f",
  };
  static const char* const kDigests8x4[kNumFilterIntraPredictors] = {
      "73f82633aeb28db1d254d077edefd8a9", "8eee505cdb5828e33b67ff5572445dac",
      "9b0f101c28c66a916079fe5ed33b4021", "47fd44a7e5a5b55f067908192698e25c",
      "eab59a3710d9bdeca8fa03a15d3f95d6",
  };
  static const char* const kDigests8x8[kNumFilterIntraPredictors] = {
      "aa07b7a007c4c1d494ddb44a23c27bcd", "d27eee43f15dfcfe4c46cd46b681983b",
      "1015d26022cf57acfdb11fd3f6b9ccb0", "4f0e00ef556fbcac2fb31e3b18869070",
      "918c2553635763a0756b20154096bca6",
  };
  static const char* const kDigests8x16[kNumFilterIntraPredictors] = {
      "a8ac58b2efb02092035cca206dbf5fbe", "0b22b000b7f124b32545bc86dd9f0142",
      "cd6a08e023cad301c084b6ec2999da63", "c017f5f4fa5c05e7638ae4db98512b13",
      "893e6995522e23ed3d613ef3797ca580",
  };
  static const char* const kDigests8x32[kNumFilterIntraPredictors] = {
      "b3d5d4f09b778ae2b8cc0e9014c22320", "e473874a1e65228707489be9ca6477aa",
      "91bda5a2d32780af345bb3d49324732f", "20f2ff26f004f02e8e2be49e6cadc32f",
      "00c909b749e36142b133a7357271e83e",
  };
  static const char* const kDigests16x4[kNumFilterIntraPredictors] = {
      "ef252f074fc3f5367748436e676e78ca", "cd436d8803ea40db3a849e7c869855c7",
      "9cd8601b5d66e61fd002f8b11bfa58d9", "b982f17ee36ef0d1c2cfea20197d5666",
      "9e350d1cd65d520194281633f566810d",
  };
  static const char* const kDigests16x8[kNumFilterIntraPredictors] = {
      "9a7e0cf9b023a89ee619ee672ba2a219", "c20186bc642912ecd4d48bc4924a79b1",
      "77de044f4c7f717f947a36fc0aa17946", "3f2fc68f11e6ee0220adb8d1ee085c8e",
      "2f37e586769dfb88d9d4116b9c28c5ab",
  };
  static const char* const kDigests16x16[kNumFilterIntraPredictors] = {
      "36c5b85b9a6b1d2e8f44f09c81adfe9c", "78494ce3a6a78aa2879ad2e24d43a005",
      "aa30cd29a74407dbec80161745161eb2", "ae2a0975ef166e05e5e8c3701bd19e93",
      "6322fba6f3bcb1f6c8e78160d200809c",
  };
  static const char* const kDigests16x32[kNumFilterIntraPredictors] = {
      "82d54732c37424946bc73f5a78f64641", "071773c82869bb103c31e05f14ed3c2f",
      "3a0094c150bd6e21ce1f17243b21e76b", "998ffef26fc65333ae407bbe9d41a252",
      "6491add6b665aafc364c8c104a6a233d",
  };
  static const char* const kDigests32x8[kNumFilterIntraPredictors] = {
      "c60062105dd727e94f744c35f0d2156e", "36a9e4d543701c4c546016e35e9c4337",
      "05a8d07fe271023e63febfb44814d114", "0a28606925519d1ed067d64761619dc8",
      "bb8c34b143910ba49b01d13e94d936ac",
  };
  static const char* const kDigests32x16[kNumFilterIntraPredictors] = {
      "60e6caeec9194fcb409469e6e1393128", "5d764ead046443eb14f76822a569b056",
      "b1bf22fcc282614354166fa1eb6e5f8b", "4b188e729fe49ae24100b3ddd8f17313",
      "75f430fdea0b7b5b66866fd68a795a6a",
  };
  static const char* const kDigests32x32[kNumFilterIntraPredictors] = {
      "5bb91a37b1979866eb23b59dd352229d", "589aa983109500749609d7be1cb79711",
      "5e8fb1927cdbe21143494b56b5d400f6", "9e28f741d19c64b2a0577d83546d32d9",
      "73c73237a5d891096066b186abf96854",
  };

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4;
    case kTransformSize4x8:
      return kDigests4x8;
    case kTransformSize4x16:
      return kDigests4x16;
    case kTransformSize8x4:
      return kDigests8x4;
    case kTransformSize8x8:
      return kDigests8x8;
    case kTransformSize8x16:
      return kDigests8x16;
    case kTransformSize8x32:
      return kDigests8x32;
    case kTransformSize16x4:
      return kDigests16x4;
    case kTransformSize16x8:
      return kDigests16x8;
    case kTransformSize16x16:
      return kDigests16x16;
    case kTransformSize16x32:
      return kDigests16x32;
    case kTransformSize32x8:
      return kDigests32x8;
    case kTransformSize32x16:
      return kDigests32x16;
    case kTransformSize32x32:
      return kDigests32x32;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(FilterIntraPredTest8bpp, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.5e8 / (block_width_ * block_height_));
  TestSpeed(GetFilterIntraPredDigests8bpp(tx_size_), num_runs);
}

TEST_P(FilterIntraPredTest8bpp, FixedInput) {
  TestSpeed(GetFilterIntraPredDigests8bpp(tx_size_), 1);
}

TEST_P(FilterIntraPredTest8bpp, Overflow) { TestSaturatedValues(); }
TEST_P(FilterIntraPredTest8bpp, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

using CflIntraPredTest8bpp = CflIntraPredTest<8, uint8_t>;

const char* GetCflIntraPredDigest8bpp(TransformSize tx_size) {
  static const char* const kDigest4x4 = "9ea7088e082867fd5ae394ca549fe1ed";
  static const char* const kDigest4x8 = "323b0b4784b6658da781398e61f2da3d";
  static const char* const kDigest4x16 = "99eb9c65f227ca7f71dcac24645a4fec";
  static const char* const kDigest8x4 = "e8e782e31c94f3974b87b93d455262d8";
  static const char* const kDigest8x8 = "23ab9fb65e7bbbdb985709e115115eb5";
  static const char* const kDigest8x16 = "52f5add2fc4bbb2ff893148645e95b9c";
  static const char* const kDigest8x32 = "283fdee9af8afdb76f72dd7339c92c3c";
  static const char* const kDigest16x4 = "eead35f515b1aa8b5175b283192b86e6";
  static const char* const kDigest16x8 = "5778e934254eaab04230bc370f64f778";
  static const char* const kDigest16x16 = "4e8ed38ccba0d62f1213171da2212ed3";
  static const char* const kDigest16x32 = "61a29bd7699e18ca6ea5641d1d023bfd";
  static const char* const kDigest32x8 = "7f31607bd4f9ec879aa47f4daf9c7bb0";
  static const char* const kDigest32x16 = "eb84dfab900fa6a90e132b186b4c6c36";
  static const char* const kDigest32x32 = "e0ff35d407cb214578d61ef419c94237";

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigest4x4;
    case kTransformSize4x8:
      return kDigest4x8;
    case kTransformSize4x16:
      return kDigest4x16;
    case kTransformSize8x4:
      return kDigest8x4;
    case kTransformSize8x8:
      return kDigest8x8;
    case kTransformSize8x16:
      return kDigest8x16;
    case kTransformSize8x32:
      return kDigest8x32;
    case kTransformSize16x4:
      return kDigest16x4;
    case kTransformSize16x8:
      return kDigest16x8;
    case kTransformSize16x16:
      return kDigest16x16;
    case kTransformSize16x32:
      return kDigest16x32;
    case kTransformSize32x8:
      return kDigest32x8;
    case kTransformSize32x16:
      return kDigest32x16;
    case kTransformSize32x32:
      return kDigest32x32;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(CflIntraPredTest8bpp, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflIntraPredDigest8bpp(tx_size_), num_runs);
}

TEST_P(CflIntraPredTest8bpp, FixedInput) {
  TestSpeed(GetCflIntraPredDigest8bpp(tx_size_), 1);
}

TEST_P(CflIntraPredTest8bpp, Overflow) { TestSaturatedValues(); }

TEST_P(CflIntraPredTest8bpp, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

using CflSubsamplerTest8bpp444 =
    CflSubsamplerTest<8, uint8_t, kSubsamplingType444>;
using CflSubsamplerTest8bpp422 =
    CflSubsamplerTest<8, uint8_t, kSubsamplingType422>;
using CflSubsamplerTest8bpp420 =
    CflSubsamplerTest<8, uint8_t, kSubsamplingType420>;

const char* GetCflSubsamplerDigest8bpp(TransformSize tx_size,
                                       SubsamplingType subsampling_type) {
  static const char* const kDigests4x4[3] = {
      "a8fa98d76cc3ccffcffc0d02dfae052c", "929cf2c23d926b500616797f8b1baf5b",
      "1d03f091956838e7f2b113aabd8b9da9"};
  static const char* const kDigests4x8[3] = {
      "717b84f867f413c87c90a7c5d0125c8c", "6ccd9f48842b1a802e128b46b8f4885d",
      "68a334f5d2abecbc78562b3280b5fb0c"};
  static const char* const kDigests4x16[3] = {
      "ecd1340b7e065dd8807fd9861abb7d99", "042c3fee17df7ef8fb8cef616f212a91",
      "b0600f0bc3fbfc374bb3628360dcae5c"};
  static const char* const kDigests8x4[3] = {
      "4ea5617f4ed8e9edc2fff88d0ab8e53f", "b02288905f218c9f54ce4a472ec7b22e",
      "3522d3a4dd3839d1a86fb39b31a86d52"};
  static const char* const kDigests8x8[3] = {
      "a0488493e6bcdb868713a95f9b4a0091", "ff6c1ac1d94fce63c282ba49186529bf",
      "082e34ba04d04d7cd6fe408823987602"};
  static const char* const kDigests8x16[3] = {
      "e01dd4bb21daaa6e991cd5b1e6f30300", "2a1b13f932e39cc5f561afea9956f47a",
      "d8d266282cb7123f780bd7266e8f5913"};
  static const char* const kDigests8x32[3] = {
      "0fc95e4ab798b95ccd2966ff75028b03", "6bc6e45ef2f664134449342fe76006ff",
      "d294fb6399edaa267aa167407c0ebccb"};
  static const char* const kDigests16x4[3] = {
      "4798c2cf649b786bd153ad88353d52aa", "43a4bfa3b8caf4b72f58c6a1d1054f64",
      "a928ebbec2db1508c8831a440d82eb98"};
  static const char* const kDigests16x8[3] = {
      "736b7f5b603cb34abcbe1b7e69b6ce93", "90422000ab20ecb519e4d277a9b3ea2b",
      "c8e71c2fddbb850c5a50592ee5975368"};
  static const char* const kDigests16x16[3] = {
      "4f15a694966ee50a9e987e9a0aa2423b", "9e31e2f5a7ce7bef738b135755e25dcd",
      "2ffeed4d592a0455f6d888913969827f"};
  static const char* const kDigests16x32[3] = {
      "3a10438bfe17ea39efad20608a0520eb", "79e8e8732a6ffc29dfbb0b3fc29c2883",
      "185ca976ccbef7fb5f3f8c6aa22d5a79"};
  static const char* const kDigests32x8[3] = {
      "683704f08839a15e42603e4977a3e815", "13d311635372aee8998fca1758e75e20",
      "9847d88eaaa57c086a2e6aed583048d3"};
  static const char* const kDigests32x16[3] = {
      "14b6761bf9f1156cf2496f532512aa99", "ee57bb7f0aa2302d29cdc1bfce72d5fc",
      "a4189655fe714b82eb88cb5092c0ad76"};
  static const char* const kDigests32x32[3] = {
      "dcfbe71b70a37418ccb90dbf27f04226", "c578556a584019c1bdc2d0c3b9fd0c88",
      "db200bc8ccbeacd6a42d6b8e5ad1d931"};

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4[subsampling_type];
    case kTransformSize4x8:
      return kDigests4x8[subsampling_type];
    case kTransformSize4x16:
      return kDigests4x16[subsampling_type];
    case kTransformSize8x4:
      return kDigests8x4[subsampling_type];
    case kTransformSize8x8:
      return kDigests8x8[subsampling_type];
    case kTransformSize8x16:
      return kDigests8x16[subsampling_type];
    case kTransformSize8x32:
      return kDigests8x32[subsampling_type];
    case kTransformSize16x4:
      return kDigests16x4[subsampling_type];
    case kTransformSize16x8:
      return kDigests16x8[subsampling_type];
    case kTransformSize16x16:
      return kDigests16x16[subsampling_type];
    case kTransformSize16x32:
      return kDigests16x32[subsampling_type];
    case kTransformSize32x8:
      return kDigests32x8[subsampling_type];
    case kTransformSize32x16:
      return kDigests32x16[subsampling_type];
    case kTransformSize32x32:
      return kDigests32x32[subsampling_type];
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(CflSubsamplerTest8bpp444, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflSubsamplerDigest8bpp(tx_size_, SubsamplingType()), num_runs);
}

TEST_P(CflSubsamplerTest8bpp444, FixedInput) {
  TestSpeed(GetCflSubsamplerDigest8bpp(tx_size_, SubsamplingType()), 1);
}

TEST_P(CflSubsamplerTest8bpp444, Overflow) { TestSaturatedValues(); }

TEST_P(CflSubsamplerTest8bpp444, Random) { TestRandomValues(); }

TEST_P(CflSubsamplerTest8bpp422, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflSubsamplerDigest8bpp(tx_size_, SubsamplingType()), num_runs);
}

TEST_P(CflSubsamplerTest8bpp422, FixedInput) {
  TestSpeed(GetCflSubsamplerDigest8bpp(tx_size_, SubsamplingType()), 1);
}

TEST_P(CflSubsamplerTest8bpp422, Overflow) { TestSaturatedValues(); }

TEST_P(CflSubsamplerTest8bpp422, Random) { TestRandomValues(); }

TEST_P(CflSubsamplerTest8bpp420, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflSubsamplerDigest8bpp(tx_size_, SubsamplingType()), num_runs);
}

TEST_P(CflSubsamplerTest8bpp420, FixedInput) {
  TestSpeed(GetCflSubsamplerDigest8bpp(tx_size_, SubsamplingType()), 1);
}

TEST_P(CflSubsamplerTest8bpp420, Overflow) { TestSaturatedValues(); }

TEST_P(CflSubsamplerTest8bpp420, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

#if LIBGAV1_MAX_BITDEPTH >= 10
using IntraPredTest10bpp = IntraPredTest<10, uint16_t>;

const char* const* GetIntraPredDigests10bpp(TransformSize tx_size) {
  static const char* const kDigests4x4[kNumIntraPredictors] = {
      "432bf9e762416bec582cb3654cbc4545", "8b9707ff4d506e0cb326f2d9a8d78705",
      "a076275258cc5af87ed8b075136fb219", "f9587004012a8d2cecaa347331ccdf96",
      "1c4e6890c5e6eed495fe54a6b6df8d6f", "0ae15fae8969a3c972ee895f325955a3",
      "97db177738b831da8066df4f3fb7adbd", "4add5685b8a56991c9dce4ff7086ec25",
      "75c6a655256188e378e70658b8f1631f", "14a27db20f9d5594ef74a7ea10c3e5ef",
  };
  static const char* const kDigests4x8[kNumIntraPredictors] = {
      "9cbd7c18aca2737fa41db27150798819", "13d1e734692e27339c10b07da33c1113",
      "0617cf74e2dd5d34ea517af1767fa47e", "c6a7b01228ccdf74af8528ef8f5f55c6",
      "13b05d87b3d566b2f7a4b332cd8a762e", "b26ae0e8da1fe8989dfe2900fa2c3847",
      "c30f3acdd386bdac91028fe48b751810", "04d2baf5192c5af97ca18d3b9b0d5968",
      "a0ef82983822fc815bf1e8326cd41e33", "20bf218bae5f6b5c6d56b85f3f9bbadb",
  };
  static const char* const kDigests4x16[kNumIntraPredictors] = {
      "d9b47bdddaa5e22312ff9ece7a3cae08", "cb76c79971b502dd8999a7047b3e2f86",
      "3b09a3ff431d03b379acfdc444602540", "88608f6fcd687831e871053723cf76c3",
      "a7bd2a17de1cf19c9a4b2c550f277a5c", "29b389f564f266a67687b8d2bc750418",
      "4680847c30fe93c06f87e2ee1da544d6", "0e4eda11e1fe6ebe8526c2a2c5390bbb",
      "bf3e20197282885acabb158f3a77ba59", "fccea71d1a253316b905f4a073c84a36",
  };
  static const char* const kDigests8x4[kNumIntraPredictors] = {
      "05ba0ed96aac48cd94e7597f12184320", "d97d04e791904d3cedc34d5430a4d1d2",
      "49217081a169c2d30b0a43f816d0b58b", "09e2a6a6bfe35b83e9434ee9c8dcf417",
      "4b03c8822169ee4fa058513d65f0e32f", "cabdeebc923837ee3f2d3480354d6a81",
      "957eda610a23a011ed25976aee94eaf0", "4a197e3dfce1f0d3870138a9b66423aa",
      "18c0d0fbe0e96a0baf2f98fa1908cbb9", "21114e5737328cdbba9940e4f85a0855",
  };
  static const char* const kDigests8x8[kNumIntraPredictors] = {
      "430e99eecda7e6434e1973dbdcc2a29d", "88864d7402c09b57735db49c58707304",
      "8312f80b936380ceb51375e29a4fd75d", "472a7ed9c68bdbd9ecca197b7a8b3f01",
      "4f66ee4dc0cb752c3b65d576cd06bb5c", "36383d6f61799143470129e2d5241a6f",
      "c96279406c8d2d02771903e93a4e8d37", "4fb64f9700ed0bf08fbe7ab958535348",
      "c008c33453ac9cf8c42ae6ec88f9941c", "39c401a9938b23e318ae7819e458daf1",
  };
  static const char* const kDigests8x16[kNumIntraPredictors] = {
      "bda6b75fedfe0705f9732ff84c918672", "4ff130a47429e0762386557018ec10b2",
      "8156557bf938d8e3a266318e57048fc5", "bdfa8e01a825ec7ae2d80519e3c94eec",
      "108fc8e5608fe09f9cc30d7a52cbc0c1", "a2271660af5424b64c6399ca5509dee1",
      "b09af9729f39516b28ff62363f8c0cb2", "4fe67869dac99048dfcf4d4e621884ec",
      "311f498369a9c98f77a961bf91e73e65", "d66e78b9f41d5ee6a4b25e37ec9af324",
  };
  static const char* const kDigests8x32[kNumIntraPredictors] = {
      "26c45325f02521e7e5c66c0aa0819329", "79dfb68513d4ccd2530c485f0367858e",
      "8288e99b4d738b13956882c3ad3f03fe", "7c4993518b1620b8be8872581bb72239",
      "2b1c3126012d981f787ed0a2601ee377", "051ba9f0c4d4fecb1fcd81fdea94cae4",
      "320362239ad402087303a4df39512bb1", "210df35b2055c9c01b9e3e5ae24e524b",
      "f8536db74ce68c0081bbd8799dac25f9", "27f2fe316854282579906d071af6b705",
  };
  static const char* const kDigests16x4[kNumIntraPredictors] = {
      "decff67721ff7e9e65ec641e78f5ccf3", "99e3b2fbdabfa9b76b749cfb6530a9fd",
      "accdb3d25629916963a069f1e1c0e061", "ad42855e9146748b0e235b8428487b4b",
      "53025e465f267e7af2896ebd028447a0", "577d26fcd2d655cc77a1f1f875648699",
      "7a61a3619267221b448b20723840e9f0", "fb4ccc569bdae3614e87bc5be1e84284",
      "b866095d8a3e6910cc4f92f8d8d6075a", "6ba9013cba1624872bfbac111e8d344a",
  };
  static const char* const kDigests16x8[kNumIntraPredictors] = {
      "2832156bd076c75f8be5622f34cb3efe", "da70e516f5a8842dd4965b80cd8d2a76",
      "c3e137c6d79c57be2073d1eda22c8d1e", "8c5d28c7b3301b50326582dd7f89a175",
      "9d8558775155b201cd178ab61458b642", "ecbddb9c6808e0c609c8fe537b7f7408",
      "29a123c22cb4020170f9a80edf1208da", "653d0cd0688aa682334156f7b4599b34",
      "1bfa66ae92a22a0346511db1713fe7df", "1802ad1e657e7fc08fc063342f471ca1",
  };
  static const char* const kDigests16x16[kNumIntraPredictors] = {
      "2270c626de9d49769660ae9184a6428f", "9f069625cdcdd856e2e7ec19ff4fcd50",
      "34167b9c413362a377aa7b1faf92ae6d", "3cec2b23d179765daea8dfb87c9efdd5",
      "daa8f0863a5df2aef2b20999961cc8f8", "d9e4dd4bc63991e4f09cb97eb25f4db4",
      "4e1a182fc3fcf5b9f5a73898f81c2004", "c58e4275406c9fd1c2a74b40c27afff0",
      "b8092796fd4e4dd9d2b92afb770129ba", "75424d1f18ff00c4093743d033c6c9b6",
  };
  static const char* const kDigests16x32[kNumIntraPredictors] = {
      "5aa050947f3d488537f5a68c23bb135b", "9e66143a2c3863b6fe171275a192d378",
      "86b0c4777625e84d52913073d234f860", "9e2144fcf2107c76cec4241416bbecd5",
      "c72be592efc72c3c86f2359b6f622aba", "c4e0e735545f78f43e21e9c39eab7b8f",
      "52122e7c84a4bab67a8a359efb427023", "7b5fd8bb7e0744e81fd6fa4ed4c2e0fb",
      "a9950d110bffb0411a8fcd1262dceef0", "2a2dd496f01f5d87f257ed202a703cbe",
  };
  static const char* const kDigests16x64[kNumIntraPredictors] = {
      "eeb1b873e81ca428b11f162bd5b28843", "39ce7d22791f82562b0ca1e0afdf1604",
      "6bd6bdac8982a4b84613f9963d35d5e9", "a9ac2438e87522621c7e6fe6d02c01ab",
      "a8b9c471fe6c66ed0717e77fea77bba1", "e050b6aa38aee6e951d3be5a94a8abd0",
      "3c5ecc31aa45e8175d37e90af247bca6", "30c0f9e412ea726970f575f910edfb94",
      "f3d96395816ce58fb98480a5b4c32ab2", "9c14811957e013fb009dcd4a3716b338",
  };
  static const char* const kDigests32x8[kNumIntraPredictors] = {
      "d6560d7fc9ae9bd7c25e2983b4a825e3", "90a67154bbdc26cd06ab0fa25fff3c53",
      "c42d37c5a634e68fafc982626842db0b", "ecc8646d258cfa431facbc0dba168f80",
      "9f3c167b790b52242dc8686c68eac389", "62dc3bc34406636ccec0941579461f65",
      "5c0f0ebdb3c936d4decc40d5261aec7c", "dbfc0f056ca25e0331042da6d292e10a",
      "14fa525d74e6774781198418d505c595", "5f95e70db03da9ed70cd79e23f19199c",
  };
  static const char* const kDigests32x16[kNumIntraPredictors] = {
      "dfe3630aa9eeb1adcc8604269a309f26", "ba6180227d09f5a573f69dc6ee1faf80",
      "03edea9d71ca3d588e1a0a69aecdf555", "2c8805415f44b4fac6692090dc1b1ddd",
      "18efd17ed72a6e92ef8b0a692cf7a2e3", "63a6e0abfb839b43c68c23b2c43c8918",
      "be15479205bb60f5a17baaa81a6b47ad", "243d21e1d9f9dd2b981292ac7769315a",
      "21de1cb5269e0e1d08930c519e676bf7", "73065b3e27e9c4a3a6d043712d3d8b25",
  };
  static const char* const kDigests32x32[kNumIntraPredictors] = {
      "c3136bb829088e33401b1affef91f692", "68bbcf93d17366db38bbc7605e07e322",
      "2786be5fb7c25eeec4d2596c4154c3eb", "25ac7468e691753b8291be859aac7493",
      "a6805ce21bfd26760e749efc8f590fa3", "5a38fd324b466e8ac43f5e289d38107e",
      "dd0628fc5cc920b82aa941378fa907c8", "8debadbdb2dec3dc7eb43927e9d36998",
      "61e1bc223c9e04c64152cc4531b6c099", "900b00ac1f20c0a8d22f8b026c0ee1cc",
  };
  static const char* const kDigests32x64[kNumIntraPredictors] = {
      "5a591b2b83f0a6cce3c57ce164a5f983", "f42167ec516102b83b2c5176df57316b",
      "58f3772d3df511c8289b340beb178d96", "c24166e7dc252d34ac6f92712956d751",
      "7dca3acfe2ea09e6292a9ece2078b827", "5c029235fc0820804e40187d2b22a96e",
      "375572944368afbc04ca97dab7fb3328", "8867235908736fd99c4022e4ed604e6e",
      "63ec336034d62846b75558c49082870f", "46f35d85eb8499d61bfeac1c49e52531",
  };
  static const char* const kDigests64x16[kNumIntraPredictors] = {
      "67755882209304659a0e6bfc324e16b9", "cd89b272fecb5f23431b3f606f590722",
      "9bcff7d971a4af0a2d1cac6d66d83482", "d8d6bb55ebeec4f03926908d391e15ba",
      "0eb5b5ced3e7177a1dd6a1e72e7a7d21", "92b47fe431d9cf66f9e601854f0f3017",
      "7dc599557eddb2ea480f86fc89c76b30", "4f40175676c164320fe8005440ad9217",
      "b00eacb24081a041127f136e9e5983ec", "cb0ab76a5e90f2eb75c38b99b9833ff8",
  };
  static const char* const kDigests64x32[kNumIntraPredictors] = {
      "21d873011d1b4ef1daedd9aa8c6938ea", "4866da21db0261f738903d97081cb785",
      "a722112233a82595a8d001a4078b834d", "24c7a133c6fcb59129c3782ef908a6c1",
      "490e40505dd255d3a909d8a72c280cbc", "2afe719fb30bf2a664829bb74c8f9e2a",
      "623adad2ebb8f23e355cd77ace4616cd", "d6092541e9262ad009bef79a5d350a86",
      "ae86d8fba088683ced8abfd7e1ddf380", "32aa8aa21f2f24333d31f99e12b95c53",
  };
  static const char* const kDigests64x64[kNumIntraPredictors] = {
      "6d88aeb40dfe3ac43c68808ca3c00806", "6a75d88ac291d6a3aaf0eec0ddf2aa65",
      "30ef52d7dc451affdd587c209f5cb2dd", "e073f7969f392258eaa907cf0636452a",
      "de10f07016a2343bcd3a9deb29f4361e", "dc35ff273fea4355d2c8351c2ed14e6e",
      "01b9a545968ac75c3639ddabb837fa0b", "85c98ed9c0ea1523a15281bc9a909b8c",
      "4c255f7ef7fd46db83f323806d79dca4", "fe2fe6ffb19cb8330e2f2534271d6522",
  };

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4;
    case kTransformSize4x8:
      return kDigests4x8;
    case kTransformSize4x16:
      return kDigests4x16;
    case kTransformSize8x4:
      return kDigests8x4;
    case kTransformSize8x8:
      return kDigests8x8;
    case kTransformSize8x16:
      return kDigests8x16;
    case kTransformSize8x32:
      return kDigests8x32;
    case kTransformSize16x4:
      return kDigests16x4;
    case kTransformSize16x8:
      return kDigests16x8;
    case kTransformSize16x16:
      return kDigests16x16;
    case kTransformSize16x32:
      return kDigests16x32;
    case kTransformSize16x64:
      return kDigests16x64;
    case kTransformSize32x8:
      return kDigests32x8;
    case kTransformSize32x16:
      return kDigests32x16;
    case kTransformSize32x32:
      return kDigests32x32;
    case kTransformSize32x64:
      return kDigests32x64;
    case kTransformSize64x16:
      return kDigests64x16;
    case kTransformSize64x32:
      return kDigests64x32;
    case kTransformSize64x64:
      return kDigests64x64;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(IntraPredTest10bpp, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetIntraPredDigests10bpp(tx_size_), num_runs);
}

TEST_P(IntraPredTest10bpp, FixedInput) {
  TestSpeed(GetIntraPredDigests10bpp(tx_size_), 1);
}

TEST_P(IntraPredTest10bpp, Overflow) { TestSaturatedValues(); }
TEST_P(IntraPredTest10bpp, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

using DirectionalIntraPredTest10bpp = DirectionalIntraPredTest<10, uint16_t>;

const char* const* GetDirectionalIntraPredDigests10bpp(TransformSize tx_size) {
  static const char* const kDigests4x4[kNumDirectionalIntraPredictors] = {
      "a683f4d7ccd978737615f61ecb4d638d",
      "90c94374eaf7e9501f197863937b8639",
      "0d3969cd081523ac6a906eecc7980c43",
  };
  static const char* const kDigests4x8[kNumDirectionalIntraPredictors] = {
      "c3ffa2979b325644e4a56c882fe27347",
      "1f61f5ee413a9a3b8d1d93869ec2aee0",
      "4795ea944779ec4a783408769394d874",
  };
  static const char* const kDigests4x16[kNumDirectionalIntraPredictors] = {
      "45c3282c9aa51024c1d64a40f230aa45",
      "5cd47dd69f8bd0b15365a0c5cfc0a49a",
      "06336c507b05f98c1d6a21abc43e6182",
  };
  static const char* const kDigests8x4[kNumDirectionalIntraPredictors] = {
      "7370476ff0abbdc5e92f811b8879c861",
      "a239a50adb28a4791b52a0dfff3bee06",
      "4779a17f958a9ca04e8ec08c5aba1d36",
  };
  static const char* const kDigests8x8[kNumDirectionalIntraPredictors] = {
      "305463f346c376594f82aad8304e0362",
      "0cd481e5bda286c87a645417569fd948",
      "48c7899dc9b7163b0b1f61b3a2b4b73e",
  };
  static const char* const kDigests8x16[kNumDirectionalIntraPredictors] = {
      "5c18fd5339be90628c82b1fb6af50d5e",
      "35eaa566ebd3bb7c903cfead5dc9ac78",
      "9fdb0e790e5965810d02c02713c84071",
  };
  static const char* const kDigests8x32[kNumDirectionalIntraPredictors] = {
      "2168d6cc858c704748b7b343ced2ac3a",
      "1d3ce273107447faafd2e55877e48ffb",
      "d344164049d1fe9b65a3ae8764bbbd37",
  };
  static const char* const kDigests16x4[kNumDirectionalIntraPredictors] = {
      "dcef2cf51abe3fe150f388a14c762d30",
      "6a810b289b1c14f8eab8ca1274e91ecd",
      "c94da7c11f3fb11963d85c8804fce2d9",
  };
  static const char* const kDigests16x8[kNumDirectionalIntraPredictors] = {
      "50a0d08b0d99b7a574bad2cfb36efc39",
      "2dcb55874db39da70c8ca1318559f9fe",
      "6390bcd30ff3bc389ecc0a0952bea531",
  };
  static const char* const kDigests16x16[kNumDirectionalIntraPredictors] = {
      "7146c83c2620935606d49f3cb5876f41",
      "2318ddf30c070a53c9b9cf199cd1b2c5",
      "e9042e2124925aa7c1b6110617cb10e8",
  };
  static const char* const kDigests16x32[kNumDirectionalIntraPredictors] = {
      "c970f401de7b7c5bb4e3ad447fcbef8f",
      "a18cc70730eecdaa31dbcf4306ff490f",
      "32c1528ad4a576a2210399d6b4ccd46e",
  };
  static const char* const kDigests16x64[kNumDirectionalIntraPredictors] = {
      "00b3f0007da2e5d01380594a3d7162d5",
      "1971af519e4a18967b7311f93efdd1b8",
      "e6139769ce5a9c4982cfab9363004516",
  };
  static const char* const kDigests32x8[kNumDirectionalIntraPredictors] = {
      "08107ad971179cc9f465ae5966bd4901",
      "b215212a3c0dfe9182c4f2e903d731f7",
      "791274416a0da87c674e1ae318b3ce09",
  };
  static const char* const kDigests32x16[kNumDirectionalIntraPredictors] = {
      "94ea6cccae35b5d08799aa003ac08ccf",
      "ae105e20e63fb55d4fd9d9e59dc62dde",
      "973d0b2358ea585e4f486e7e645c5310",
  };
  static const char* const kDigests32x32[kNumDirectionalIntraPredictors] = {
      "d14c695c4853ddf5e5d8256bc1d1ed60",
      "6bd0ebeb53adecc11442b1218b870cb7",
      "e03bc402a9999aba8272275dce93e89f",
  };
  static const char* const kDigests32x64[kNumDirectionalIntraPredictors] = {
      "b21a8a8723758392ee659eeeae518a1e",
      "e50285454896210ce44d6f04dfde05a7",
      "f0f8ea0c6c2acc8d7d390927c3a90370",
  };
  static const char* const kDigests64x16[kNumDirectionalIntraPredictors] = {
      "ce51db16fd4fa56e601631397b098c89",
      "aa87a8635e02c1e91d13158c61e443f6",
      "4c1ee3afd46ef34bd711a34d0bf86f13",
  };
  static const char* const kDigests64x32[kNumDirectionalIntraPredictors] = {
      "25aaf5971e24e543e3e69a47254af777",
      "eb6f444b3df127d69460778ab5bf8fc1",
      "2f846cc0d506f90c0a58438600819817",
  };
  static const char* const kDigests64x64[kNumDirectionalIntraPredictors] = {
      "b26ce5b5f4b5d4a438b52e5987877fb8",
      "35721a00a70938111939cf69988d928e",
      "0af7ec35939483fac82c246a13845806",
  };

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4;
    case kTransformSize4x8:
      return kDigests4x8;
    case kTransformSize4x16:
      return kDigests4x16;
    case kTransformSize8x4:
      return kDigests8x4;
    case kTransformSize8x8:
      return kDigests8x8;
    case kTransformSize8x16:
      return kDigests8x16;
    case kTransformSize8x32:
      return kDigests8x32;
    case kTransformSize16x4:
      return kDigests16x4;
    case kTransformSize16x8:
      return kDigests16x8;
    case kTransformSize16x16:
      return kDigests16x16;
    case kTransformSize16x32:
      return kDigests16x32;
    case kTransformSize16x64:
      return kDigests16x64;
    case kTransformSize32x8:
      return kDigests32x8;
    case kTransformSize32x16:
      return kDigests32x16;
    case kTransformSize32x32:
      return kDigests32x32;
    case kTransformSize32x64:
      return kDigests32x64;
    case kTransformSize64x16:
      return kDigests64x16;
    case kTransformSize64x32:
      return kDigests64x32;
    case kTransformSize64x64:
      return kDigests64x64;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(DirectionalIntraPredTest10bpp, DISABLED_Speed) {
  const auto num_runs = static_cast<int>(5e7 / (block_width_ * block_height_));
  for (int i = kZone1; i < kNumZones; ++i) {
    TestSpeed(GetDirectionalIntraPredDigests10bpp(tx_size_),
              static_cast<Zone>(i), num_runs);
  }
}

TEST_P(DirectionalIntraPredTest10bpp, FixedInput) {
  for (int i = kZone1; i < kNumZones; ++i) {
    TestSpeed(GetDirectionalIntraPredDigests10bpp(tx_size_),
              static_cast<Zone>(i), 1);
  }
}

TEST_P(DirectionalIntraPredTest10bpp, Overflow) { TestSaturatedValues(); }

//------------------------------------------------------------------------------

using FilterIntraPredTest10bpp = FilterIntraPredTest<10, uint16_t>;

const char* const* GetFilterIntraPredDigests10bpp(TransformSize tx_size) {
  static const char* const kDigests4x4[kNumFilterIntraPredictors] = {
      "13a9014d9e255cde8e3e85abf6ef5151", "aee33aa3f3baec87a8c019743fff40f1",
      "fdd8ca2be424501f51fcdb603c2e757c", "aed00c082d1980d4bab45e9318b939f0",
      "1b363db246aa5400f49479b7d5d41799",
  };
  static const char* const kDigests4x8[kNumFilterIntraPredictors] = {
      "e718b9e31ba3da0392fd4b6cfba5d882", "31ba22989cdc3bb80749685f42c6c697",
      "6bc5b3a55b94018117569cfdced17bf9", "ec29979fb4936116493dfa1cfc93901c",
      "c6bcf564e63c42148d9917f089566432",
  };
  static const char* const kDigests4x16[kNumFilterIntraPredictors] = {
      "404bddd88dff2c0414b5398287e54f18", "ff4fb3039cec6c9ffed6d259cbbfd854",
      "7d6fa3ed9e728ff056a73c40bb6edeb6", "82845d942ad8048578e0037336905146",
      "f3c07ea65db08c639136a5a9270f95ff",
  };
  static const char* const kDigests8x4[kNumFilterIntraPredictors] = {
      "2008981638f27ba9123973a733e46c3d", "47efecf1f7628cbd8c22e168fcceb5ce",
      "04c857ffbd1edd6e2788b17410a4a39c", "deb0236c4277b4d7b174fba407e1c9d7",
      "5b58567f94ae9fa930f700c68c17399d",
  };
  static const char* const kDigests8x8[kNumFilterIntraPredictors] = {
      "d9bab44a6d1373e758bfa0ee88239093", "29b10ddb32d9de2ff0cad6126f010ff6",
      "1a03f9a18bdbab0811138cd969bf1f93", "e3273c24e77095ffa033a073f5bbcf7b",
      "5187bb3df943d154cb01fb2f244ff86f",
  };
  static const char* const kDigests8x16[kNumFilterIntraPredictors] = {
      "a2199f792634a56f1c4e88510e408773", "8fd8a98969d19832975ee7131cca9dbb",
      "d897380941f75b04b1327e63f136d7d6", "d36f52a157027d53b15b7c02a7983436",
      "0a8c23047b0364f5687b62b01f043359",
  };
  static const char* const kDigests8x32[kNumFilterIntraPredictors] = {
      "5b74ea8e4f60151cf2db9b23d803a2e2", "e0d6bb5fa7d181589c31fcf2755d7c0b",
      "42e590ffc88b8940b7aade22e13bbb6a", "e47c39ec1761aa7b5a9b1368ede7cfdc",
      "6e963a89beac6f3a362c269d1017f9a8",
  };
  static const char* const kDigests16x4[kNumFilterIntraPredictors] = {
      "9eaa079622b5dd95ad3a8feb68fa9bbb", "17e3aa6a0034e9eedcfc65b8ce6e7205",
      "eac5a5337dbaf9bcbc3d320745c8e190", "c6ba9a7e518be04f725bc1dbd399c204",
      "19020b82ce8bb49a511820c7e1d58e99",
  };
  static const char* const kDigests16x8[kNumFilterIntraPredictors] = {
      "2d2c3255d5dfc1479a5d82a7d5a0d42e", "0fbb4ee851b4ee58c6d30dd820d19e38",
      "fa77a1b056e8dc8efb702c7832531b32", "186269ca219dc663ad9b4a53e011a54b",
      "c12180a6dcde0c3579befbb5304ff70b",
  };
  static const char* const kDigests16x16[kNumFilterIntraPredictors] = {
      "dbb81d7ee7d3c83c271400d0160b2e83", "4da656a3ef238d90bb8339471a6fdb7e",
      "d95006bf299b84a1b04e38d5fa8fb4f7", "742a03331f0fbd66c57df0ae31104aca",
      "4d20aa440e38b6b7ac83c8c54d313169",
  };
  static const char* const kDigests16x32[kNumFilterIntraPredictors] = {
      "6247730c93789cc25bcb837781dfa05b", "9a93e14b06dd145e35ab21a0353bdebe",
      "6c5866353e30296a67d9bd7a65d6998d", "389d7f038d7997871745bb1305156ff9",
      "e7640d81f891e1d06e7da75c6ae74d93",
  };
  static const char* const kDigests32x8[kNumFilterIntraPredictors] = {
      "68f3a603b7c25dd78deffe91aef22834", "48c735e4aa951d6333d99e571bfeadc8",
      "35239df0993a429fc599a3037c731e4b", "ba7dd72e04af1a1fc1b30784c11df783",
      "78e9017f7434665d32ec59795aed0012",
  };
  static const char* const kDigests32x16[kNumFilterIntraPredictors] = {
      "8cf2f11f7f77901cb0c522ad191eb998", "204c76d68c5117b89b5c3a05d5548883",
      "f3751e41e7a595f43d8aaf9a40644e05", "81ea1a7d608d7b91dd3ede0f87e750ee",
      "b5951334dfbe6229d828e03cd2d98538",
  };
  static const char* const kDigests32x32[kNumFilterIntraPredictors] = {
      "9d8630188c3d1a4f28a6106e343c9380", "c6c92e059faa17163522409b7bf93230",
      "62e4c959cb06ec661d98769981fbd555", "01e61673f11011571246668e36cc61c5",
      "4530222ea1de546e202630fcf43f4526",
  };

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4;
    case kTransformSize4x8:
      return kDigests4x8;
    case kTransformSize4x16:
      return kDigests4x16;
    case kTransformSize8x4:
      return kDigests8x4;
    case kTransformSize8x8:
      return kDigests8x8;
    case kTransformSize8x16:
      return kDigests8x16;
    case kTransformSize8x32:
      return kDigests8x32;
    case kTransformSize16x4:
      return kDigests16x4;
    case kTransformSize16x8:
      return kDigests16x8;
    case kTransformSize16x16:
      return kDigests16x16;
    case kTransformSize16x32:
      return kDigests16x32;
    case kTransformSize32x8:
      return kDigests32x8;
    case kTransformSize32x16:
      return kDigests32x16;
    case kTransformSize32x32:
      return kDigests32x32;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(FilterIntraPredTest10bpp, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.5e8 / (block_width_ * block_height_));
  TestSpeed(GetFilterIntraPredDigests10bpp(tx_size_), num_runs);
}

TEST_P(FilterIntraPredTest10bpp, FixedInput) {
  TestSpeed(GetFilterIntraPredDigests10bpp(tx_size_), 1);
}

TEST_P(FilterIntraPredTest10bpp, Overflow) { TestSaturatedValues(); }

//------------------------------------------------------------------------------

using CflIntraPredTest10bpp = CflIntraPredTest<10, uint16_t>;

const char* GetCflIntraPredDigest10bpp(TransformSize tx_size) {
  static const char* const kDigest4x4 = "b4ca5f6fbb643a94eb05d59976d44c5d";
  static const char* const kDigest4x8 = "040139b76ee22af05c56baf887d3d43b";
  static const char* const kDigest4x16 = "4a1d59ace84ff07e68a0d30e9b1cebdd";
  static const char* const kDigest8x4 = "c2c149cea5fdcd18bfe5c19ec2a8aa90";
  static const char* const kDigest8x8 = "68ad90bd6f409548fa5551496b7cb0d0";
  static const char* const kDigest8x16 = "bdc54eff4de8c5d597b03afaa705d3fe";
  static const char* const kDigest8x32 = "362aebc6d68ff0d312d55dcd6a8a927d";
  static const char* const kDigest16x4 = "349e813aedd211581c5e64ba1938eaa7";
  static const char* const kDigest16x8 = "35c64f6da17f836618b5804185cf3eef";
  static const char* const kDigest16x16 = "95be0c78dbd8dda793c62c6635b4bfb7";
  static const char* const kDigest16x32 = "4752b9eda069854d3f5c56d3f2057e79";
  static const char* const kDigest32x8 = "dafc5e973e4b6a55861f4586a11b7dd1";
  static const char* const kDigest32x16 = "1e177ed3914a165183916aca1d01bb74";
  static const char* const kDigest32x32 = "4c9ab3cf9baa27bb34e29729dabc1ea6";

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigest4x4;
    case kTransformSize4x8:
      return kDigest4x8;
    case kTransformSize4x16:
      return kDigest4x16;
    case kTransformSize8x4:
      return kDigest8x4;
    case kTransformSize8x8:
      return kDigest8x8;
    case kTransformSize8x16:
      return kDigest8x16;
    case kTransformSize8x32:
      return kDigest8x32;
    case kTransformSize16x4:
      return kDigest16x4;
    case kTransformSize16x8:
      return kDigest16x8;
    case kTransformSize16x16:
      return kDigest16x16;
    case kTransformSize16x32:
      return kDigest16x32;
    case kTransformSize32x8:
      return kDigest32x8;
    case kTransformSize32x16:
      return kDigest32x16;
    case kTransformSize32x32:
      return kDigest32x32;
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(CflIntraPredTest10bpp, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflIntraPredDigest10bpp(tx_size_), num_runs);
}

TEST_P(CflIntraPredTest10bpp, FixedInput) {
  TestSpeed(GetCflIntraPredDigest10bpp(tx_size_), 1);
}

TEST_P(CflIntraPredTest10bpp, Overflow) { TestSaturatedValues(); }

TEST_P(CflIntraPredTest10bpp, Random) { TestRandomValues(); }

//------------------------------------------------------------------------------

using CflSubsamplerTest10bpp444 =
    CflSubsamplerTest<10, uint16_t, kSubsamplingType444>;
using CflSubsamplerTest10bpp422 =
    CflSubsamplerTest<10, uint16_t, kSubsamplingType422>;
using CflSubsamplerTest10bpp420 =
    CflSubsamplerTest<10, uint16_t, kSubsamplingType420>;

const char* GetCflSubsamplerDigest10bpp(TransformSize tx_size,
                                        SubsamplingType subsampling_type) {
  static const char* const kDigests4x4[3] = {
      "a8abcad9a6c9b046a100689135a108cb", "01081c2a0d0c15dabdbc725be5660451",
      "93d1d9df2861240d88f5618e42178654"};
  static const char* const kDigests4x8[3] = {
      "d1fd8cd0709ca6634ad85f3e331672e1", "0d603fcc910aca3db41fc7f64e826c27",
      "cf88b6d1b7b025cfa0082361775aeb75"};
  static const char* const kDigests4x16[3] = {
      "ce2e036a950388a564d8637b1416a6c6", "6c36c46cd72057a6b36bc12188b6d22c",
      "0884a0e53384cd5173035ad8966d8f2f"};
  static const char* const kDigests8x4[3] = {
      "174e961983ed71fb105ed71aa3f9daf5", "330946cc369a534618a1014b4e3f6f18",
      "8070668aa389c1d09f8aaf43c1223e8c"};
  static const char* const kDigests8x8[3] = {
      "86884feb35217010f73ccdbadecb635e", "b8cbc646e1bf1352e5b4b599eaef1193",
      "4a1110382e56b42d3b7a4132bccc01ee"};
  static const char* const kDigests8x16[3] = {
      "a694c4e1f89648ffb49efd6a1d35b300", "864b9da67d23a2f8284b28b2a1e5aa30",
      "bd012ca1cea256dd02c231339a4cf200"};
  static const char* const kDigests8x32[3] = {
      "60c42201bc24e518c1a3b3b6306d8125", "4d530e47c2b7555d5f311ee910d61842",
      "71888b17b832ef55c0cd9449c0e6b077"};
  static const char* const kDigests16x4[3] = {
      "6b6d5ae4cc294c070ce65ab31c5a7d4f", "0fbecee20d294939e7a0183c2b4a0b96",
      "917cd884923139d5c05a11000722e3b6"};
  static const char* const kDigests16x8[3] = {
      "688c41726d9ac35fb5b18c57bca76b9c", "d439a2e0a60d672b644cd1189e2858b9",
      "edded6d166a77a6c3ff46fddc13f372f"};
  static const char* const kDigests16x16[3] = {
      "feb2bad9f6bb3f60eaeaf6c1bfd89ca5", "d65cabce5fcd9a29d1dfc530e4764f3a",
      "2f1a91898812d2c9320c7506b3a72eb4"};
  static const char* const kDigests16x32[3] = {
      "6f23b1851444d29633e62ce77bf09559", "4a449fd078bd0c9657cdc24b709c0796",
      "e44e18cb8bda2d34b52c96d5b6b510be"};
  static const char* const kDigests32x8[3] = {
      "77bf9ba56f7e1d2f04068a8a00b139da", "a85a1dea82963dedab9a2f7ad4169b5f",
      "d12746071bee96ddc075c6368bc9fbaf"};
  static const char* const kDigests32x16[3] = {
      "cce3422f7f8cf57145f979359ac92f98", "1c18738d40bfa91296e5fdb7230bf9a7",
      "02513142d109aee10f081cacfb33d1c5"};
  static const char* const kDigests32x32[3] = {
      "789008e49d0276de186af968196dd4a7", "b8848b00968a7ba4787765b7214da05f",
      "12d13828db57605b00ce99469489651d"};

  switch (tx_size) {
    case kTransformSize4x4:
      return kDigests4x4[subsampling_type];
    case kTransformSize4x8:
      return kDigests4x8[subsampling_type];
    case kTransformSize4x16:
      return kDigests4x16[subsampling_type];
    case kTransformSize8x4:
      return kDigests8x4[subsampling_type];
    case kTransformSize8x8:
      return kDigests8x8[subsampling_type];
    case kTransformSize8x16:
      return kDigests8x16[subsampling_type];
    case kTransformSize8x32:
      return kDigests8x32[subsampling_type];
    case kTransformSize16x4:
      return kDigests16x4[subsampling_type];
    case kTransformSize16x8:
      return kDigests16x8[subsampling_type];
    case kTransformSize16x16:
      return kDigests16x16[subsampling_type];
    case kTransformSize16x32:
      return kDigests16x32[subsampling_type];
    case kTransformSize32x8:
      return kDigests32x8[subsampling_type];
    case kTransformSize32x16:
      return kDigests32x16[subsampling_type];
    case kTransformSize32x32:
      return kDigests32x32[subsampling_type];
    default:
      ADD_FAILURE() << "Unknown transform size: " << tx_size;
      return nullptr;
  }
}

TEST_P(CflSubsamplerTest10bpp444, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflSubsamplerDigest10bpp(tx_size_, SubsamplingType()), num_runs);
}

TEST_P(CflSubsamplerTest10bpp444, FixedInput) {
  TestSpeed(GetCflSubsamplerDigest10bpp(tx_size_, SubsamplingType()), 1);
}

TEST_P(CflSubsamplerTest10bpp444, Overflow) { TestSaturatedValues(); }

TEST_P(CflSubsamplerTest10bpp444, Random) { TestRandomValues(); }

TEST_P(CflSubsamplerTest10bpp422, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflSubsamplerDigest10bpp(tx_size_, SubsamplingType()), num_runs);
}

TEST_P(CflSubsamplerTest10bpp422, FixedInput) {
  TestSpeed(GetCflSubsamplerDigest10bpp(tx_size_, SubsamplingType()), 1);
}

TEST_P(CflSubsamplerTest10bpp422, Overflow) { TestSaturatedValues(); }

TEST_P(CflSubsamplerTest10bpp422, Random) { TestRandomValues(); }

TEST_P(CflSubsamplerTest10bpp420, DISABLED_Speed) {
  const auto num_runs =
      static_cast<int>(2.0e9 / (block_width_ * block_height_));
  TestSpeed(GetCflSubsamplerDigest10bpp(tx_size_, SubsamplingType()), num_runs);
}

TEST_P(CflSubsamplerTest10bpp420, FixedInput) {
  TestSpeed(GetCflSubsamplerDigest10bpp(tx_size_, SubsamplingType()), 1);
}

TEST_P(CflSubsamplerTest10bpp420, Overflow) { TestSaturatedValues(); }

TEST_P(CflSubsamplerTest10bpp420, Random) { TestRandomValues(); }

#endif  // LIBGAV1_MAX_BITDEPTH >= 10

constexpr TransformSize kTransformSizes[] = {
    kTransformSize4x4,   kTransformSize4x8,   kTransformSize4x16,
    kTransformSize8x4,   kTransformSize8x8,   kTransformSize8x16,
    kTransformSize8x32,  kTransformSize16x4,  kTransformSize16x8,
    kTransformSize16x16, kTransformSize16x32, kTransformSize16x64,
    kTransformSize32x8,  kTransformSize32x16, kTransformSize32x32,
    kTransformSize32x64, kTransformSize64x16, kTransformSize64x32,
    kTransformSize64x64};

// Filter-intra and Cfl predictors are available only for transform sizes
// with max(width, height) <= 32.
constexpr TransformSize kTransformSizesSmallerThan32x32[] = {
    kTransformSize4x4,   kTransformSize4x8,   kTransformSize4x16,
    kTransformSize8x4,   kTransformSize8x8,   kTransformSize8x16,
    kTransformSize8x32,  kTransformSize16x4,  kTransformSize16x8,
    kTransformSize16x16, kTransformSize16x32, kTransformSize32x8,
    kTransformSize32x16, kTransformSize32x32};

INSTANTIATE_TEST_SUITE_P(C, IntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(C, DirectionalIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(C, FilterIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflSubsamplerTest8bpp444,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflSubsamplerTest8bpp422,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflSubsamplerTest8bpp420,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, IntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(SSE41, DirectionalIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(SSE41, FilterIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(SSE41, CflIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(SSE41, CflSubsamplerTest8bpp444,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(SSE41, CflSubsamplerTest8bpp420,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
#endif  // LIBGAV1_ENABLE_SSE4_1
#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, IntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(NEON, DirectionalIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(NEON, FilterIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(NEON, CflIntraPredTest8bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(NEON, CflSubsamplerTest8bpp444,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(NEON, CflSubsamplerTest8bpp420,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
#endif  // LIBGAV1_ENABLE_NEON

#if LIBGAV1_MAX_BITDEPTH >= 10
INSTANTIATE_TEST_SUITE_P(C, IntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(C, DirectionalIntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(C, FilterIntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflIntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflSubsamplerTest10bpp444,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflSubsamplerTest10bpp422,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(C, CflSubsamplerTest10bpp420,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, IntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(SSE41, DirectionalIntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(SSE41, CflIntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(SSE41, CflSubsamplerTest10bpp444,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(SSE41, CflSubsamplerTest10bpp420,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
#endif  // LIBGAV1_ENABLE_SSE4_1
#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, IntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizes));
INSTANTIATE_TEST_SUITE_P(NEON, CflIntraPredTest10bpp,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(NEON, CflSubsamplerTest10bpp444,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
INSTANTIATE_TEST_SUITE_P(NEON, CflSubsamplerTest10bpp420,
                         ::testing::ValuesIn(kTransformSizesSmallerThan32x32));
#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp

static std::ostream& operator<<(std::ostream& os, const TransformSize tx_size) {
  return os << dsp::kTransformSizeNames[tx_size];
}

}  // namespace libgav1
