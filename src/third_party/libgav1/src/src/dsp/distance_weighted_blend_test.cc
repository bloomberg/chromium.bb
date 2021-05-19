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

#include "src/dsp/distance_weighted_blend.h"

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
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/cpu.h"
#include "src/utils/memory.h"
#include "tests/third_party/libvpx/acm_random.h"
#include "tests/utils.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr int kNumSpeedTests = 500000;

constexpr int kQuantizedDistanceLookup[4][2] = {
    {9, 7}, {11, 5}, {12, 4}, {13, 3}};

struct TestParam {
  TestParam(int width, int height) : width(width), height(height) {}
  int width;
  int height;
};

std::ostream& operator<<(std::ostream& os, const TestParam& param) {
  return os << "BlockSize" << param.width << "x" << param.height;
}

template <int bitdepth, typename Pixel>
class DistanceWeightedBlendTest : public ::testing::TestWithParam<TestParam>,
                                  public test_utils::MaxAlignedAllocable {
 public:
  DistanceWeightedBlendTest() = default;
  ~DistanceWeightedBlendTest() override = default;

  void SetUp() override {
    test_utils::ResetDspTable(bitdepth);
    DistanceWeightedBlendInit_C();
    const dsp::Dsp* const dsp = dsp::GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_func_ = dsp->distance_weighted_blend;
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const absl::string_view test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      base_func_ = nullptr;
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        DistanceWeightedBlendInit_SSE4_1();
      }
    } else if (absl::StartsWith(test_case, "NEON/")) {
      DistanceWeightedBlendInit_NEON();
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
    func_ = dsp->distance_weighted_blend;
  }

 protected:
  void Test(const char* digest, int num_tests);

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
  dsp::DistanceWeightedBlendFunc base_func_;
  dsp::DistanceWeightedBlendFunc func_;
};

template <int bitdepth, typename Pixel>
void DistanceWeightedBlendTest<bitdepth, Pixel>::Test(const char* digest,
                                                      int num_tests) {
  if (func_ == nullptr) return;
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  PredType* src_1 = source1_;
  PredType* src_2 = source2_;

  const int index = rnd.Rand8() & 3;
  const uint8_t weight_0 = kQuantizedDistanceLookup[index][0];
  const uint8_t weight_1 = kQuantizedDistanceLookup[index][1];
  // In libgav1, predictors have an offset which are later subtracted and
  // clipped in distance weighted blending. Therefore we add the offset
  // here to match libaom's implementation.
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      // distance_weighted_blend is applied to compound prediction values. This
      // implies a range far exceeding that of pixel values.
      // The ranges include kCompoundOffset in 10bpp and 12bpp.
      // see: src/dsp/convolve.cc & src/dsp/warp.cc.
      static constexpr int kCompoundPredictionRange[3][2] = {
          // 8bpp
          {-5132, 9212},
          // 10bpp
          {3988, 61532},
          // 12bpp
          {3974, 61559},
      };
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
    func_(source1_, source2_, weight_0, weight_1, width_, height_, dest_,
          sizeof(Pixel) * kDestStride);
    elapsed_time += absl::Now() - start;
  }

  test_utils::CheckMd5Digest(
      "DistanceWeightedBlend",
      absl::StrFormat("BlockSize%dx%d", width_, height_).c_str(), digest, dest_,
      sizeof(dest_), elapsed_time);
}

const TestParam kTestParam[] = {
    TestParam(4, 4),    TestParam(4, 8),    TestParam(4, 16),
    TestParam(8, 4),    TestParam(8, 8),    TestParam(8, 16),
    TestParam(8, 32),   TestParam(16, 4),   TestParam(16, 8),
    TestParam(16, 16),  TestParam(16, 32),  TestParam(16, 64),
    TestParam(32, 8),   TestParam(32, 16),  TestParam(32, 32),
    TestParam(32, 64),  TestParam(32, 128), TestParam(64, 16),
    TestParam(64, 32),  TestParam(64, 64),  TestParam(64, 128),
    TestParam(128, 32), TestParam(128, 64), TestParam(128, 128),
};

const char* GetDistanceWeightedBlendDigest8bpp(const TestParam block_size) {
  static const char* const kDigestsWidth4[] = {
      "ebf389f724f8ab46a2cac895e4e073ca",
      "09acd567b6b12c8cf8eb51d8b86eb4bf",
      "57bb4d65695d8ec6752f2bd8686b64fd",
  };
  static const char* const kDigestsWidth8[] = {
      "270905ac76f9a2cba8a552eb0bf7c8c1",
      "f0801c8574d2c271ef2bbea77a1d7352",
      "e761b580e3312be33a227492a233ce72",
      "ff214dab1a7e98e2285961d6421720c6",
  };
  static const char* const kDigestsWidth16[] = {
      "4f712609a36e817f9752326d58562ff8", "14243f5c5f7c7104160c1f2cef0a0fbc",
      "3ac3f3161b7c8dd8436b02abfdde104a", "81a00b704e0e41a5dbe6436ac70c098d",
      "af8fd02017c7acdff788be742d700baa",
  };
  static const char* const kDigestsWidth32[] = {
      "ee34332c66a6d6ed8ce64031aafe776c", "b5e3d22bd2dbdb624c8b86a1afb5ce6d",
      "607ffc22098d81b7e37a7bf62f4af5d3", "3823dbf043b4682f56d5ca698e755ea5",
      "57f7e8d1e67645269ce760a2c8da4afc",
  };
  static const char* const kDigestsWidth64[] = {
      "4acf556b921956c2bc24659cd5128401",
      "a298c544c9c3b27924b4c23cc687ea5a",
      "539e2df267782ce61c70103b23b7d922",
      "3b0cb2a0b5d384efee4d81401025bec1",
  };
  static const char* const kDigestsWidth128[] = {
      "d71ee689a40ff5f390d07717df4b7233",
      "8b56b636dd712c2f8d138badb7219991",
      "8cfc8836908902b8f915639b7bff45b3",
  };
  const int height_index =
      FloorLog2(block_size.height) - FloorLog2(block_size.width) + 2;
  switch (block_size.width) {
    case 4:
      return kDigestsWidth4[height_index - 2];
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

using DistanceWeightedBlendTest8bpp = DistanceWeightedBlendTest<8, uint8_t>;

TEST_P(DistanceWeightedBlendTest8bpp, Blending) {
  Test(GetDistanceWeightedBlendDigest8bpp(GetParam()), 1);
}

TEST_P(DistanceWeightedBlendTest8bpp, DISABLED_Speed) {
  Test(GetDistanceWeightedBlendDigest8bpp(GetParam()), kNumSpeedTests);
}

INSTANTIATE_TEST_SUITE_P(C, DistanceWeightedBlendTest8bpp,
                         ::testing::ValuesIn(kTestParam));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, DistanceWeightedBlendTest8bpp,
                         ::testing::ValuesIn(kTestParam));
#endif

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, DistanceWeightedBlendTest8bpp,
                         ::testing::ValuesIn(kTestParam));
#endif

#if LIBGAV1_MAX_BITDEPTH >= 10
const char* GetDistanceWeightedBlendDigest10bpp(const TestParam block_size) {
  static const char* const kDigestsWidth4[] = {
      "55f594b56e16d5c401274affebbcc3d3",
      "69df14da4bb33a8f7d7087921008e919",
      "1b61f33604c54015794198a13bfebf46",
  };
  static const char* const kDigestsWidth8[] = {
      "825a938185b152f7cf09bf1c0723ce2b",
      "85ea315c51d979bc9b45834d6b40ec6f",
      "92ebde208e8c39f7ec6de2de82182dbb",
      "520f84716db5b43684dbb703806383fe",
  };
  static const char* const kDigestsWidth16[] = {
      "12ca23e3e2930005a0511646e8c83da4", "6208694a6744f4a3906f58c1add670e3",
      "a33d63889df989a3bbf84ff236614267", "34830846ecb0572a98bbd192fed02b16",
      "34bb2f79c0bd7f9a80691b8af597f2a8",
  };
  static const char* const kDigestsWidth32[] = {
      "fa97f2d0e3143f1f44d3ac018b0d696d", "3df4a22456c9ab6ed346ab1b9750ae7d",
      "6276a058b35c6131bc0c94a4b4a37ebc", "9ca42da5d2d5eb339df03ae2c7a26914",
      "2ff0dc010a7b40830fb47423a9beb894",
  };
  static const char* const kDigestsWidth64[] = {
      "800e692c520f99223bc24c1ac95a0166",
      "818b6d20426585ef7fe844015a03aaf5",
      "fb48691ccfff083e01d74826e88e613f",
      "0bd350bc5bc604a224d77a5f5a422698",
  };
  static const char* const kDigestsWidth128[] = {
      "02aac5d5669c1245da876c5440c4d829",
      "a130840813cd6bd69d09bcf5f8d0180f",
      "6ece1846bea55e8f8f2ed7fbf73718de",
  };
  const int height_index =
      FloorLog2(block_size.height) - FloorLog2(block_size.width) + 2;
  switch (block_size.width) {
    case 4:
      return kDigestsWidth4[height_index - 2];
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

using DistanceWeightedBlendTest10bpp = DistanceWeightedBlendTest<10, uint16_t>;

TEST_P(DistanceWeightedBlendTest10bpp, Blending) {
  Test(GetDistanceWeightedBlendDigest10bpp(GetParam()), 1);
}

TEST_P(DistanceWeightedBlendTest10bpp, DISABLED_Speed) {
  Test(GetDistanceWeightedBlendDigest10bpp(GetParam()), kNumSpeedTests);
}

INSTANTIATE_TEST_SUITE_P(C, DistanceWeightedBlendTest10bpp,
                         ::testing::ValuesIn(kTestParam));

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, DistanceWeightedBlendTest10bpp,
                         ::testing::ValuesIn(kTestParam));
#endif
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
