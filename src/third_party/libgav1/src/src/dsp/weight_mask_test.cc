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

#include "src/dsp/weight_mask.h"

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
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

constexpr int kNumSpeedTests = 50000;
constexpr int kMaxPredictionSize = 128;
// weight_mask is only used with kCompoundPredictionTypeDiffWeighted with
// convolve producing the most extreme ranges, see: src/dsp/convolve.cc &
// src/dsp/warp.cc.
constexpr int kPredictionRange[3][2] = {
    {-5132, 9212},
    {3988, 61532},
    {3974, 61559},
};

const char* GetDigest8bpp(int id) {
  static const char* const kDigest[] = {
      "25a1d6d1b3e75213e12800676686703e",
      "b93b38e538dcb072e4b492a781f909ca",
      "50b5e6680ecdaa95c4e95c220abe5bd8",
      "" /*kBlock16x4*/,
      "fdc4a868311d629c99507f728f56d575",
      "b6da56bbefac4ca4edad1b8f68791606",
      "2dbe65f1cfbe37134bf1dbff11c222f2",
      "6d77edaf6fa479a669a6309722d8f352",
      "4f58c12179012ae1cd1c21e01258a39b",
      "9b3e1ce01d886db45d1295878c3b9e00",
      "97b5be2d7bb19a045b3815a972b918b7",
      "5b2cba7e06155bb4e9e281d6668633df",
      "ca6ea9f694ebfc6fc0c9fc4d22d140ec",
      "0efca5b9f6e5c287ff8683c558382987",
      "36941879ee00efb746c45cad08d6559b",
      "6d8ee22d7dd051f391f295c4fdb617d7",
      "e99ada080a5ddf9df50544301d0b6f6e",
      "acd821f8e49d47a0735ed1027f43d64b",

      // mask_is_inverse = true.
      "c9cd4ae74ed092198f812e864cfca8a2",
      "77125e2710c78613283fe279f179b59d",
      "52df4dae64ef06f913351c61d1082e93",
      "" /*kBlock16x4*/,
      "1fa861d6ca726db3b6ac4fa78bdcb465",
      "4f42297f3fb4cfc3afc3b89b30943c32",
      "730fefde2cd8d65ae8ceca7fb1d1e9f3",
      "cc53bf23217146c77797d3c21fac35b8",
      "55be7f6f22c02f43ccced3131c8ba02b",
      "bf1e12cd57424aee4a35969ad72cbdd3",
      "bea31fa1581e19b7819400f417130ec3",
      "fb42a215163ee9e13b9d7db1838caca2",
      "0747f7ab50b564ad30d73381337ed845",
      "74f5bdb72ae505376596c2d91fd67d27",
      "56b5053da761ffbfd856677bbc34e353",
      "15001c7c9b585e19de875ec6926c2451",
      "35d49b7ec45c42b84fdb30f89ace00fb",
      "9fcb7a44be4ce603a95978acf0fb54d7",
  };
  return kDigest[id];
}

#if LIBGAV1_MAX_BITDEPTH >= 10
const char* GetDigest10bpp(int id) {
  static const char* const kDigest[] = {
      "2e6af811d5d713b6a7611e72f59a39d5",
      "ea78d18a3cec473808a1f3684795dc2f",
      "723cd34c38218b52c664a970cb8f27e1",
      "" /*kBlock4x16*/,
      "0e814d8ef49c8e1cff792ddd59132f5d",
      "62a27fc2b487344fcd30cc4a4c90910b",
      "5b8d13908d6bdd82eabb91e4480d39eb",
      "078df1c16998a966988e0a0af7a487fa",
      "88b0157fd7d44450ff0f168183db7bb4",
      "1a80228fc6f8c696186444acc1a04770",
      "15f6d374038944e584bc6f8e066ecd99",
      "5e681103b12ae2c4885bb4c83973adcb",
      "a9663defc560891bf932c9171b68fd40",
      "74cad2af7ca7a07acf2f3d0ec03bffbf",
      "eb41ade44a8acc08398d080f800ceba4",
      "3639622c3a4faf2456d00d123a75cf0a",
      "bcc3d307c888ef478c24834fe06d5f8e",
      "b9f7fe6ab328341eb454a43b43bb233c",

      // mask_is_inverse = true.
      "d98c05f4fa4c7d1f5c4436e432fc9c25",
      "247843f989c944e54fbc96cb35dcb9b5",
      "6625bd02e234ded3e62564ac64e3004f",
      "" /*kBlock4x16*/,
      "2b7599e924341bd1004b158ca484962d",
      "4c43a668e7c3fe2d7b0623586cf4b026",
      "da62544d622857a71d261dab085133b8",
      "27d1de302169f94d517c582a6628a6dc",
      "6067534a5095b8a2e170f559c60f7dbe",
      "2837afb8f0773a617c02cae3b6865c6f",
      "2a865cefb0a88a2fa91e490dad1576a9",
      "2d9d6952a5983723297580b83b3af586",
      "69364a17514fe5fd68d2e52a546b89f8",
      "3cc7d2ddb2e0517fc3ce4a352acf9091",
      "fd20a0102bc7c9d0f0da439b46cca96b",
      "446a532584fda09a7f83a4f954c86e76",
      "f08954414023c9a942fd92b6c6a33fea",
      "c9e73849bb2203d5b9a8a7725f1e2b70",
  };
  return kDigest[id];
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

struct WeightMaskTestParam {
  WeightMaskTestParam(int width, int height, bool mask_is_inverse)
      : width(width), height(height), mask_is_inverse(mask_is_inverse) {}
  int width;
  int height;
  bool mask_is_inverse;
};

std::ostream& operator<<(std::ostream& os, const WeightMaskTestParam& param) {
  return os << param.width << "x" << param.height
            << ", mask_is_inverse: " << param.mask_is_inverse;
}

template <int bitdepth>
class WeightMaskTest : public ::testing::TestWithParam<WeightMaskTestParam>,
                       public test_utils::MaxAlignedAllocable {
 public:
  WeightMaskTest() = default;
  ~WeightMaskTest() override = default;

  void SetUp() override {
    test_utils::ResetDspTable(bitdepth);
    WeightMaskInit_C();
    const dsp::Dsp* const dsp = dsp::GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    const int width_index = FloorLog2(width_) - 3;
    const int height_index = FloorLog2(height_) - 3;
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
    } else if (absl::StartsWith(test_case, "NEON/")) {
      WeightMaskInit_NEON();
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      WeightMaskInit_SSE4_1();
    }
    func_ = dsp->weight_mask[width_index][height_index][mask_is_inverse_];
  }

 protected:
  void SetInputData(bool use_fixed_values, int value_1, int value_2);
  void Test(int num_runs, bool use_fixed_values, int value_1, int value_2);

 private:
  const int width_ = GetParam().width;
  const int height_ = GetParam().height;
  const bool mask_is_inverse_ = GetParam().mask_is_inverse;
  alignas(
      kMaxAlignment) uint16_t block_1_[kMaxPredictionSize * kMaxPredictionSize];
  alignas(
      kMaxAlignment) uint16_t block_2_[kMaxPredictionSize * kMaxPredictionSize];
  uint8_t mask_[kMaxPredictionSize * kMaxPredictionSize] = {};
  dsp::WeightMaskFunc func_;
};

template <int bitdepth>
void WeightMaskTest<bitdepth>::SetInputData(const bool use_fixed_values,
                                            const int value_1,
                                            const int value_2) {
  if (use_fixed_values) {
    std::fill(block_1_, block_1_ + kMaxPredictionSize * kMaxPredictionSize,
              value_1);
    std::fill(block_2_, block_2_ + kMaxPredictionSize * kMaxPredictionSize,
              value_2);
  } else {
    constexpr int offset = (bitdepth == 8) ? -kPredictionRange[0][0] : 0;
    constexpr int bitdepth_index = (bitdepth - 8) >> 1;
    libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
    auto get_value = [&rnd](int min, int max) {
      int value;
      do {
        value = rnd(max + 1);
      } while (value < min);
      return value;
    };
    const int min = kPredictionRange[bitdepth_index][0] + offset;
    const int max = kPredictionRange[bitdepth_index][1] + offset;

    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        block_1_[y * width_ + x] = get_value(min, max) - offset;
        block_2_[y * width_ + x] = get_value(min, max) - offset;
      }
    }
  }
}

BlockSize DimensionsToBlockSize(int width, int height) {
  if (width == 4) {
    if (height == 4) return kBlock4x4;
    if (height == 8) return kBlock4x8;
    if (height == 16) return kBlock4x16;
    return kBlockInvalid;
  }
  if (width == 8) {
    if (height == 4) return kBlock8x4;
    if (height == 8) return kBlock8x8;
    if (height == 16) return kBlock8x16;
    if (height == 32) return kBlock8x32;
    return kBlockInvalid;
  }
  if (width == 16) {
    if (height == 4) return kBlock16x4;
    if (height == 8) return kBlock16x8;
    if (height == 16) return kBlock16x16;
    if (height == 32) return kBlock16x32;
    if (height == 64) return kBlock16x64;
    return kBlockInvalid;
  }
  if (width == 32) {
    if (height == 8) return kBlock32x8;
    if (height == 16) return kBlock32x16;
    if (height == 32) return kBlock32x32;
    if (height == 64) return kBlock32x64;
    return kBlockInvalid;
  }
  if (width == 64) {
    if (height == 16) return kBlock64x16;
    if (height == 32) return kBlock64x32;
    if (height == 64) return kBlock64x64;
    if (height == 128) return kBlock64x128;
    return kBlockInvalid;
  }
  if (width == 128) {
    if (height == 64) return kBlock128x64;
    if (height == 128) return kBlock128x128;
    return kBlockInvalid;
  }
  return kBlockInvalid;
}

template <int bitdepth>
void WeightMaskTest<bitdepth>::Test(const int num_runs,
                                    const bool use_fixed_values,
                                    const int value_1, const int value_2) {
  if (func_ == nullptr) return;
  SetInputData(use_fixed_values, value_1, value_2);
  const absl::Time start = absl::Now();
  for (int i = 0; i < num_runs; ++i) {
    func_(block_1_, block_2_, mask_, kMaxPredictionSize);
  }
  const absl::Duration elapsed_time = absl::Now() - start;
  if (use_fixed_values) {
    int fixed_value = (value_1 - value_2 == 0) ? 38 : 64;
    if (mask_is_inverse_) fixed_value = 64 - fixed_value;
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        ASSERT_EQ(static_cast<int>(mask_[y * kMaxPredictionSize + x]),
                  fixed_value)
            << "x: " << x << " y: " << y;
      }
    }
  } else {
    const int id_offset = mask_is_inverse_ ? kMaxBlockSizes - 4 : 0;
    const int id = id_offset +
                   static_cast<int>(DimensionsToBlockSize(width_, height_)) - 4;
    if (bitdepth == 8) {
      test_utils::CheckMd5Digest(
          absl::StrFormat("BlockSize %dx%d", width_, height_).c_str(),
          "WeightMask", GetDigest8bpp(id), mask_, sizeof(mask_), elapsed_time);
#if LIBGAV1_MAX_BITDEPTH >= 10
    } else {
      test_utils::CheckMd5Digest(
          absl::StrFormat("BlockSize %dx%d", width_, height_).c_str(),
          "WeightMask", GetDigest10bpp(id), mask_, sizeof(mask_), elapsed_time);
#endif
    }
  }
}

const WeightMaskTestParam weight_mask_test_param[] = {
    WeightMaskTestParam(8, 8, false),     WeightMaskTestParam(8, 16, false),
    WeightMaskTestParam(8, 32, false),    WeightMaskTestParam(16, 8, false),
    WeightMaskTestParam(16, 16, false),   WeightMaskTestParam(16, 32, false),
    WeightMaskTestParam(16, 64, false),   WeightMaskTestParam(32, 8, false),
    WeightMaskTestParam(32, 16, false),   WeightMaskTestParam(32, 32, false),
    WeightMaskTestParam(32, 64, false),   WeightMaskTestParam(64, 16, false),
    WeightMaskTestParam(64, 32, false),   WeightMaskTestParam(64, 64, false),
    WeightMaskTestParam(64, 128, false),  WeightMaskTestParam(128, 64, false),
    WeightMaskTestParam(128, 128, false), WeightMaskTestParam(8, 8, true),
    WeightMaskTestParam(8, 16, true),     WeightMaskTestParam(8, 32, true),
    WeightMaskTestParam(16, 8, true),     WeightMaskTestParam(16, 16, true),
    WeightMaskTestParam(16, 32, true),    WeightMaskTestParam(16, 64, true),
    WeightMaskTestParam(32, 8, true),     WeightMaskTestParam(32, 16, true),
    WeightMaskTestParam(32, 32, true),    WeightMaskTestParam(32, 64, true),
    WeightMaskTestParam(64, 16, true),    WeightMaskTestParam(64, 32, true),
    WeightMaskTestParam(64, 64, true),    WeightMaskTestParam(64, 128, true),
    WeightMaskTestParam(128, 64, true),   WeightMaskTestParam(128, 128, true),
};

using WeightMaskTest8bpp = WeightMaskTest<8>;

TEST_P(WeightMaskTest8bpp, FixedValues) {
  const int min = kPredictionRange[0][0];
  const int max = kPredictionRange[0][1];
  Test(1, true, min, min);
  Test(1, true, min, max);
  Test(1, true, max, min);
  Test(1, true, max, max);
}

TEST_P(WeightMaskTest8bpp, RandomValues) { Test(1, false, -1, -1); }

TEST_P(WeightMaskTest8bpp, DISABLED_Speed) {
  Test(kNumSpeedTests, false, -1, -1);
}

INSTANTIATE_TEST_SUITE_P(C, WeightMaskTest8bpp,
                         ::testing::ValuesIn(weight_mask_test_param));
#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, WeightMaskTest8bpp,
                         ::testing::ValuesIn(weight_mask_test_param));
#endif
#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, WeightMaskTest8bpp,
                         ::testing::ValuesIn(weight_mask_test_param));
#endif

#if LIBGAV1_MAX_BITDEPTH >= 10
using WeightMaskTest10bpp = WeightMaskTest<10>;

TEST_P(WeightMaskTest10bpp, FixedValues) {
  const int min = kPredictionRange[1][0];
  const int max = kPredictionRange[1][1];
  Test(1, true, min, min);
  Test(1, true, min, max);
  Test(1, true, max, min);
  Test(1, true, max, max);
}

TEST_P(WeightMaskTest10bpp, RandomValues) { Test(1, false, -1, -1); }

TEST_P(WeightMaskTest10bpp, DISABLED_Speed) {
  Test(kNumSpeedTests, false, -1, -1);
}

INSTANTIATE_TEST_SUITE_P(C, WeightMaskTest10bpp,
                         ::testing::ValuesIn(weight_mask_test_param));
#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, WeightMaskTest10bpp,
                         ::testing::ValuesIn(weight_mask_test_param));
#endif
#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, WeightMaskTest10bpp,
                         ::testing::ValuesIn(weight_mask_test_param));
#endif
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
