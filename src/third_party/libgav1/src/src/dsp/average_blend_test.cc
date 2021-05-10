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
  static constexpr int kDestStride = kMaxSuperBlockSizeInPixels;
  const int width_ = GetParam().width;
  const int height_ = GetParam().height;
  alignas(kMaxAlignment) uint16_t
      source1_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels];
  alignas(kMaxAlignment) uint16_t
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
  uint16_t* src_1 = source1_;
  uint16_t* src_2 = source2_;
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  const int mask = (1 << (bitdepth + (kFilterBits - kRoundBitsHorizontal))) - 1;
  const int compound_round_offset = (bitdepth == 8) ? 0 : kCompoundOffset;
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      src_1[x] = rnd.Rand16() & mask;
      src_2[x] = rnd.Rand16() & mask;
      src_1[x] += compound_round_offset;
      src_2[x] += compound_round_offset;
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
      "6a02bbcc2a43b3c69974ff17fec6b8e6", "612b27128271ab284155194efb4ca3e8"};
  static const char* const kDigestsWidth8[] = {
      "c22ef0eafdf4a8ced020e2e046712cce", "6124bd376645b59ada1e9e5c0d215d67"};
  static const char* const kDigestsWidth16[] = {
      "4b2a188124bd078be581afd89f12fa58", "789607b15e344f3a63be44473ddaed40",
      "932929b60516b14204de08c382497116"};
  static const char* const kDigestsWidth32[] = {
      "08a7a14cbcf877be410c1175e3d34203", "27b7132ee24253389780900f39af223d",
      "8a228ecc87389b62175a0d805f212eb8"};
  static const char* const kDigestsWidth64[] = {
      "0fc63c59c2f9d35e8afa52d940d75f54", "53ca3fa951f731b58a009e7e426858cd",
      "3a380228c00f49b4bf7361c67210bfa9"};
  static const char* const kDigestsWidth128[] = {
      "f844aa786a7e00ed64c720b809bf99ee", "9e65c896a40ae91934f52b4f8715dcae"};
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
      "c8697e7bd1227c9b7af598663b2ce982", "385c5d0198830490adcb5f7eb3eb1017"};
  static const char* const kDigestsWidth8[] = {
      "2dc3a8cec04e1651ed490eeb22b89434", "7a299a39263f767f15f4c5503ec004c4"};
  static const char* const kDigestsWidth16[] = {
      "1c73e2d626c7ec77d2d765de3e3b467e", "1cb3620c2b933cff1dd139edfef8a77c",
      "730182dfb816aa295b15096cbc7e14f4"};
  static const char* const kDigestsWidth32[] = {
      "6fba0faf129a93de6c9fc6dbf1311c91", "a777d092ef11fec52311aef295b91633",
      "1a2d478b58430185c352a2864e8cd30b"};
  static const char* const kDigestsWidth64[] = {
      "1974a1c0640ef07f478bc89abf1bc480", "408da09ccf5fec0a3003cbe9fb6e1a97",
      "d924ad272cdbb43efbfb0dc281b64e84"};
  static const char* const kDigestsWidth128[] = {
      "12bc0024cc10cd664a0ffa0283033245", "df11aa6fe7f9f2dc98e893079bd0bb94"};
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
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
