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
  dsp::DistanceWeightedBlendFunc base_func_;
  dsp::DistanceWeightedBlendFunc func_;
};

template <int bitdepth, typename Pixel>
void DistanceWeightedBlendTest<bitdepth, Pixel>::Test(const char* digest,
                                                      int num_tests) {
  if (func_ == nullptr) return;
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  uint16_t* src_1 = source1_;
  uint16_t* src_2 = source2_;
  const int compound_round_offset = (bitdepth == 8) ? 0 : kCompoundOffset;
  const int mask = (1 << bitdepth) - 1;
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      src_1[x] = rnd.Rand16() & mask;
      src_2[x] = rnd.Rand16() & mask;
    }
    src_1 += width_;
    src_2 += width_;
  }
  const int index = rnd.Rand8() & 3;
  const uint8_t weight_0 = kQuantizedDistanceLookup[index][0];
  const uint8_t weight_1 = kQuantizedDistanceLookup[index][1];
  src_1 = source1_;
  src_2 = source2_;
  // In libgav1, predictors have an offset which are later subtracted and
  // clipped in distance weighted blending. Therefore we add the offset
  // here to match libaom's implementation.
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      src_1[x] = rnd.Rand16() & mask;
      src_2[x] = rnd.Rand16() & mask;
      // Tweak the input to match libaom.
      src_1[x] <<= 4;
      src_2[x] <<= 4;
      // In libgav1, predictors have an offset which are later subtracted and
      // clipped in distance weighted blending. Therefore we add the offset
      // here to match libaom's implementation.
      src_1[x] += compound_round_offset;
      src_2[x] += compound_round_offset;
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
      "6a2c94e8e9382b567156a4a108a6a827", "a35689437df51359159d0e5975c4684a",
      "d13e83101173da3df72a06e0c6609429"};
  static const char* const kDigestsWidth8[] = {
      "86a00fa110c47be0d335f5c32e719ee5", "e2c5c6f08e8b581ffb69882b163effc4",
      "e49b3d913273e1f9c073041c7805a08d", "d79df57558542c83e9a02b4a4c883702"};
  static const char* const kDigestsWidth16[] = {
      "98b066f37ceb12bcd90a1abc3d35a56c", "b5ebfea4af16c36e304884ec86901f6f",
      "a86e798abdfa9098d6260ca37c35a72e", "3a6afb2133b22c4582efd1d1b7e63a97",
      "44675ce8d82c58d1dd1010dec668845e"};
  static const char* const kDigestsWidth32[] = {
      "82dba8eef1f20ac476fa0ba69a5c8041", "5289aef739ee4169697070f099625090",
      "256e870e3fa930dc6edfdaa967f40263", "33a86afaba1fbdb13b1b38cff094e332",
      "f2c197f3bcc8ff077dafa8c48f92be0b"};
  static const char* const kDigestsWidth64[] = {
      "e6da635ce252a3a5a52850caa9a47683", "addc101b8bbb8cc43be479e77f9d01db",
      "4f1629f00f0c50117d21f76a581813c5", "72adca8d14f7662a9eeecf4b372b4952"};
  static const char* const kDigestsWidth128[] = {
      "9e41fd8ce034d312b808381d443b40b2", "4c63699198404cb80611508a4f4cec88",
      "e546f39118cf69f011f8cb1f7751c9c3"};
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
      "7d227af2d7d47fa36a43c910d4f1afeb", "dcc14ebe2e60fe3165ea066108daa186",
      "b5ed1e296ea7da13b1e797edbfbf7531"};
  static const char* const kDigestsWidth8[] = {
      "3651b557ecc64cce211faddc0d8fdfb3", "1ae3d5d059c883f0a8fbd46a8428d41f",
      "437a70cbc8b8e69f25e2c90082be0872", "bea9eaf591e982be46319eaa82f2348b"};
  static const char* const kDigestsWidth16[] = {
      "b55cdb4ae880d55f942675212b0f6cf7", "11f0f422da267ab5beea6a07d96e59b5",
      "a8a48c4ed58797d0b74741e6ae38d9d8", "26e932dbdbdbf43474c733bb529ba8a8",
      "bc660c72d6d68fd50b2416a7688e7c51"};
  static const char* const kDigestsWidth32[] = {
      "3137b264303168628fd7ec5687ee6e64", "03f493d7864c77960701cab4d9f875ae",
      "2d0d1ba3b80aeb3a5025846b442ed2fd", "068a972bc21ef71ebd211ba82e084a1e",
      "fb5a0e0f2f602d3b3c2111dd7fd7c09c"};
  static const char* const kDigestsWidth64[] = {
      "71c905c4e0758bf0c7919d038188a57f", "194957ac92ddbb1cf3dba8ab8b5bc0ed",
      "8121f5720a0a6875179c014893522554", "b0a78fc63959988a06130a43dc361768"};
  static const char* const kDigestsWidth128[] = {
      "48499fac6af3ddf7df9d8034922f1ce6", "09d36c3b2aa67b704bf79d98eb580170",
      "6865b5d9fbf71f31dcb61dd7e67881b9"};
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
