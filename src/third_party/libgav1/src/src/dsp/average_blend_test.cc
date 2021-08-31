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

#include "src/dsp/average_blend.h"

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "src/dsp/constants.h"
#include "src/dsp/distance_weighted_blend.h"
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

constexpr int kNumSpeedTests = 5e8;
constexpr char kAverageBlend[] = "AverageBlend";
// average_blend is applied to compound prediction values. This implies a range
// far exceeding that of pixel values.
// The ranges include kCompoundOffset in 10bpp and 12bpp.
// see: src/dsp/convolve.cc & src/dsp/warp.cc.
constexpr int kCompoundPredictionRange[3][2] = {
    // 8bpp
    {-5132, 9212},
    // 10bpp
    {3988, 61532},
    // 12bpp
    {3974, 61559},
};

struct TestParam {
  TestParam(int width, int height) : width(width), height(height) {}
  int width;
  int height;
};

std::ostream& operator<<(std::ostream& os, const TestParam& param) {
  return os << "BlockSize" << param.width << "x" << param.height;
}

template <int bitdepth, typename Pixel>
class AverageBlendTest : public ::testing::TestWithParam<TestParam>,
                         public test_utils::MaxAlignedAllocable {
 public:
  AverageBlendTest() = default;
  ~AverageBlendTest() override = default;

  void SetUp() override {
    test_utils::ResetDspTable(bitdepth);
    AverageBlendInit_C();
    DistanceWeightedBlendInit_C();
    const dsp::Dsp* const dsp = dsp::GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_func_ = dsp->average_blend;
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const absl::string_view test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      base_func_ = nullptr;
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        AverageBlendInit_SSE4_1();
      }
    } else if (absl::StartsWith(test_case, "NEON/")) {
      AverageBlendInit_NEON();
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
    func_ = dsp->average_blend;
    dist_blend_func_ = dsp->distance_weighted_blend;
  }

 protected:
  void Test(const char* digest, int num_tests, bool debug);

 private:
  using PredType =
      typename std::conditional<bitdepth == 8, int16_t, uint16_t>::type;
  static constexpr int kDestStride = kMaxSuperBlockSizeInPixels;
  const int width_ = GetParam().width;
  const int height_ = GetParam().height;
  alignas(kMaxAlignment) PredType
      source1_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels];
  alignas(kMaxAlignment) PredType
      source2_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels];
  Pixel dest_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels] = {};
  Pixel reference_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels] =
      {};
  dsp::AverageBlendFunc base_func_;
  dsp::AverageBlendFunc func_;
  dsp::DistanceWeightedBlendFunc dist_blend_func_;
};

template <int bitdepth, typename Pixel>
void AverageBlendTest<bitdepth, Pixel>::Test(const char* digest, int num_tests,
                                             bool debug) {
  if (func_ == nullptr) return;
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  PredType* src_1 = source1_;
  PredType* src_2 = source2_;
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      constexpr int bitdepth_index = (bitdepth - 8) >> 1;
      const int min_val = kCompoundPredictionRange[bitdepth_index][0];
      const int max_val = kCompoundPredictionRange[bitdepth_index][1];
      src_1[x] = static_cast<PredType>(rnd(max_val - min_val) + min_val);
      src_2[x] = static_cast<PredType>(rnd(max_val - min_val) + min_val);
    }
    src_1 += width_;
    src_2 += width_;
  }
  absl::Duration elapsed_time;
  for (int i = 0; i < num_tests; ++i) {
    const absl::Time start = absl::Now();
    func_(source1_, source2_, width_, height_, dest_,
          sizeof(dest_[0]) * kDestStride);
    elapsed_time += absl::Now() - start;
  }
  if (debug) {
    if (base_func_ != nullptr) {
      base_func_(source1_, source2_, width_, height_, reference_,
                 sizeof(reference_[0]) * kDestStride);
    } else {
      // Use dist_blend_func_ as the base for C tests.
      const int8_t weight = 8;
      dist_blend_func_(source1_, source2_, weight, weight, width_, height_,
                       reference_, sizeof(reference_[0]) * kDestStride);
    }
    EXPECT_TRUE(test_utils::CompareBlocks(dest_, reference_, width_, height_,
                                          kDestStride, kDestStride, false));
  }

  test_utils::CheckMd5Digest(
      kAverageBlend, absl::StrFormat("%dx%d", width_, height_).c_str(), digest,
      dest_, sizeof(dest_[0]) * kDestStride * height_, elapsed_time);
}

const TestParam kTestParam[] = {
    TestParam(4, 4),    TestParam(4, 8),    TestParam(8, 8),
    TestParam(8, 16),   TestParam(16, 8),   TestParam(16, 16),
    TestParam(16, 32),  TestParam(32, 16),  TestParam(32, 32),
    TestParam(32, 64),  TestParam(64, 32),  TestParam(64, 64),
    TestParam(64, 128), TestParam(128, 64), TestParam(128, 128),
};

using AverageBlendTest8bpp = AverageBlendTest<8, uint8_t>;

const char* GetAverageBlendDigest8bpp(const TestParam block_size) {
  static const char* const kDigestsWidth4[] = {
      "152bcc35946900b1ed16369b3e7a81b7",
      "c23e9b5698f7384eaae30a3908118b77",
  };
  static const char* const kDigestsWidth8[] = {
      "d90d3abd368e58c513070a88b34649ba",
      "77f7d53d0edeffb3537afffd9ff33a4a",
  };
  static const char* const kDigestsWidth16[] = {
      "a50e268e93b48ae39cc2a47d377410e2",
      "65c8502ff6d78065d466f9911ed6bb3e",
      "bc2c873b9f5d74b396e1df705e87f699",
  };
  static const char* const kDigestsWidth32[] = {
      "ca40d46d89773e7f858b15fcecd43cc0",
      "bfdc894707323f4dc43d1326309f8368",
      "f4733417621719b7feba3166ec0da5b9",
  };
  static const char* const kDigestsWidth64[] = {
      "db38fe2e082bd4a09acb3bb1d52ee11e",
      "3ad44401cc731215c46c9b7d96f7e4ae",
      "6c43267be5ed03d204a05fe36090f870",
  };
  static const char* const kDigestsWidth128[] = {
      "c8cfe46ebf166c1cbf08e8804206aadb",
      "b0557b5156d2334c8ce4a7ee12f9d6b4",
  };
  // height < width implies 0.
  // height == width implies 1.
  // height > width implies 2.
  const int height_index = block_size.height / block_size.width;
  switch (block_size.width) {
    case 4:
      return kDigestsWidth4[height_index - 1];
    case 8:
      return kDigestsWidth8[height_index - 1];
    case 16:
      return kDigestsWidth16[height_index];
    case 32:
      return kDigestsWidth32[height_index];
    case 64:
      return kDigestsWidth64[height_index];
    default:
      EXPECT_EQ(block_size.width, 128)
          << "Unknown width parameter: " << block_size.width;
      return kDigestsWidth128[height_index];
  }
}

TEST_P(AverageBlendTest8bpp, Blending) {
  Test(GetAverageBlendDigest8bpp(GetParam()), 1, false);
}

TEST_P(AverageBlendTest8bpp, DISABLED_Speed) {
  Test(GetAverageBlendDigest8bpp(GetParam()),
       kNumSpeedTests / (GetParam().height * GetParam().width), false);
}

INSTANTIATE_TEST_SUITE_P(C, AverageBlendTest8bpp,
                         ::testing::ValuesIn(kTestParam));
#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, AverageBlendTest8bpp,
                         ::testing::ValuesIn(kTestParam));
#endif
#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AverageBlendTest8bpp,
                         ::testing::ValuesIn(kTestParam));
#endif

#if LIBGAV1_MAX_BITDEPTH >= 10
using AverageBlendTest10bpp = AverageBlendTest<10, uint16_t>;

const char* GetAverageBlendDigest10bpp(const TestParam block_size) {
  static const char* const kDigestsWidth4[] = {
      "98c0671c092b4288adcaaa17362cc4a3",
      "7083f3def8bfb63ab3a985ef5616a923",
  };
  static const char* const kDigestsWidth8[] = {
      "3bee144b9ea6f4288b860c24f88a22f3",
      "27113bd17bf95034f100e9046c7b59d2",
  };
  static const char* const kDigestsWidth16[] = {
      "24c9e079b9a8647a6ee03f5441f2cdd9",
      "dd05777751ccdb4356856c90e1176e53",
      "27b1d69d035b1525c013b7373cfe3875",
  };
  static const char* const kDigestsWidth32[] = {
      "efd24dd7b555786bff1a482e51170ea3",
      "3b37ddac87de443cd18784f02c2d1dd5",
      "80d8070939a743a20689a65bf5dc0a68",
  };
  static const char* const kDigestsWidth64[] = {
      "af1fe8c52487c9f2951c3ea516828abb",
      "ea6f18ff56b053748c18032b7e048e83",
      "af0cb87fe27d24c2e0afd2c90a8533a6",
  };
  static const char* const kDigestsWidth128[] = {
      "16a83b19911d6dc7278a694b8baa9901",
      "bd22e77ce6fa727267ff63eeb4dcb19c",
  };
  // (height  < width) -> 0
  // (height == width) -> 1
  // (height  > width) -> 2
  const int height_index = block_size.height / block_size.width;
  switch (block_size.width) {
    case 4:
      return kDigestsWidth4[height_index - 1];
    case 8:
      return kDigestsWidth8[height_index - 1];
    case 16:
      return kDigestsWidth16[height_index];
    case 32:
      return kDigestsWidth32[height_index];
    case 64:
      return kDigestsWidth64[height_index];
    default:
      EXPECT_EQ(block_size.width, 128)
          << "Unknown width parameter: " << block_size.width;
      return kDigestsWidth128[height_index];
  }
}

TEST_P(AverageBlendTest10bpp, Blending) {
  Test(GetAverageBlendDigest10bpp(GetParam()), 1, false);
}

TEST_P(AverageBlendTest10bpp, DISABLED_Speed) {
  Test(GetAverageBlendDigest10bpp(GetParam()),
       kNumSpeedTests / (GetParam().height * GetParam().width) / 2, false);
}

INSTANTIATE_TEST_SUITE_P(C, AverageBlendTest10bpp,
                         ::testing::ValuesIn(kTestParam));
#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, AverageBlendTest10bpp,
                         ::testing::ValuesIn(kTestParam));
#endif
#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AverageBlendTest10bpp,
                         ::testing::ValuesIn(kTestParam));
#endif
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
