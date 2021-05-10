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

#include "src/dsp/mask_blend.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
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

constexpr int kNumSpeedTests = 50000;

const char* GetDigest8bpp(int id) {
  static const char* const kDigest[] = {
      "2dd91daba84f0375fa4e44de367f8b46", "55db73529f0df3bf4010e992dd8a5c86",
      "d1ca1bd7071b1424a252e8cde151b7f4", "e08984aa08f33f59457b514e3d1a3561",
      "4867911cf8b77be85c90930ec7639394", "a64e0898a07fc5b6fa0eb4c38dbae8a0",
      "308b60ac3b4f39abef376532dbe8d74b", "1258aecd878e6631f3dc70e79c9e45ad",
      "b6633f1590acd24388166db11f5aac25", "0bc90f4f5f985f7f6726ca9ee5628d22",
      "668e5cc427b3d9a9291a79ecdab8da13", "cfcbd5951477be105538ec3386b374cb",
      "281e6ff77b4d883c399f162219d5b7c9", "65ea38314ee61e5433b087d690a770e4",
      "abfefb8cb4860c4cbf5e8231f4a6c933", "7af7a3d74e0b5c5241d3294cd506be32",
      "a03e5ce8414b04870fa7f49fab428e6b", "89e80848db8e70adba77129c455c481f",
      "9415252f054e5cb357b5b3737894af22", "2dcfe14af3bdb30efa235b423e63e404",
      "46276a31e8f7f0ad360412a98eca8487", "a36fd522feb8da32def45883a8d03b50",
      "a82475352dc23d2e0038e2737ccaa417", "15c130c26c6ea07c70a086bc82adfff6",
      "1dc1ba6f9dd23bc3c86ea687436987e5", "77834da899998a970aea5ccaabae89fa",
      "429d0e3f34b32c8cbbd885f79803e388", "0e18016afda6aa3a6b1d89e6f14dd731",
      "70246aed8008bb3cfc2eca8f4c402fa0", "7e8d35f59825cf114ed36238563ef124",
      "2386b1b89819acef49754b19e975483b", "a0b15dbea4821a133b0ad0f9d77d9f5e",
      "d9858411ae3c0371d9041492159e4be3", "30ecc05cd7d82841a327c2fda07af877",
      "73da76ef0ad534e2e432ddd9891fdc08", "dadb0b3d2a706a144748367988abb10f",
      "67b25e85ff6d41488a42b87bb99a9c3e", "7c9288b35b6f90305a53a3b87889cfd0",
      "659e28b852dd982762d1add0286f81ef", "af5e6c65ed48a0eb7d509f7036398728",
      "f9da3310d7dc75910483dfdd2af6ee62", "a9423b4d67bee5e7c7bc3baa7a9c017a",
      "6b90a04333407013dd011c1af582e79f", "e658088a74bfb7cc57a2faa74a6f8689",
      "6eedf27126eba6915035f9f701a1b992", "89116a7c6ad3f70a5b3f3105d04ad1a8",
      "f41e5e166b049d0006d8b2cab56523b3", "3bed57a684075bbe3c25fd0c3e5520c3",
      "85c0b21af2afb18ce948abfe3e23c85b", "bd8aaa3602d6b42438f8449f8adb52cb",
      "1266bad904caad2c6d4047abefc2393d", "6573f2fe2a14c9ab7d5e192742388489",
      "6b9b443f6306059fa3fe18df9de6dc48", "c9a91ee6ae8b653f552866e4073dd097",
      "fa58938384198f7709d4871d155ba100", "033d121fc782e83ff94c31e73407d2a8",
      "7ea268d79f7b8c75a4feeb24e892471a", "73a376bb3e07172d1e094ab8e01a7d42",
      "13c366e0da1663fac126ea3d3876c110", "2f5eb5fcdf953c63fee2b8c75a6e5568",
      "2054b197f002223f2d75699884279511", "67ce53e6991657a922d77cc8a23f1e07",
      "f48e6d666435e7a917d6f90539b0d557", "21d03669d8d255e43552f8fb90724717",
      "43dbaa1a7aaf2a01764e78e041b6763b", "a8173347ea861ecee6da54f81df73951",
      "6b97ec4e4647a8de026d693059b855b7", "a85bf4c4b48791ac4971339877e4bc8a",
      "04cf84d020a60ce3ce53845255ca8ec9", "ddd87035b960499b883d0aefcf96b6b2",
      "278c5dd102474d598bf788cd66977ba9", "78b3790785811516142d417a49177c8c",
      "7883ea9c2df0b4f5797cba31f4352678", "727004811025ac97b04940e2eaf68f94",
      "7ffa3f97ec13dc8b6225550133a392bc", "6f5f2cb7a44aa0daea5c6b3315110591",
      "88a59d68875fb44ec3be9d3fa293bccb", "0516e71f76b9d998794d3d63e480fa2f",
      "193793d42f0964b4a958a68d9d7eb4ba", "4d259c7c6a95744e4ebaaa5361befb11",
      "c090155b997dc103203bcb5a9dcc6282",
  };
  return kDigest[id];
}

#if LIBGAV1_MAX_BITDEPTH >= 10
const char* GetDigest10bpp(int id) {
  static const char* const kDigest[] = {
      "8196b9f210352077cc679e446759da78", "efe97b629240d3684ca42ab9b5d61b34",
      "004a82797cc5def32c1790552c540051", "307a96332b52d8cd98dad8d317986ba2",
      "2e1a7c36304a6cfacf39a2e214970a48", "55875ec75bdb3db4afcfc2566eee8acb",
      "b55f12d10b3cb9d71b9c67fa1815bb17", "b5413338cc13d4dd040a188093f2871f",
      "8fc31544ea7a4dcf54875cc758d9deca", "5a2d915e31b07d07834057a48eab1526",
      "542a39b7abf911e43bdabb26994a2c6d", "ae2dca20ce113a6465d6db29f3a0c0b4",
      "7933aa47fc4e73e742b36f94131060c4", "0e9511aebeda0d20a5379f1f6ade758a",
      "1eb2e41f3b2875927694353c85eb46b2", "6a2c23d4753ae26bc12ee62aec28be2d",
      "82c9a494a6bb9a8896a1a598208a1a8b", "98b253d1062509fd6930f5116a2122d0",
      "382eea1848495702f3c7b249d0494984", "06e261d242bbccd58bfa3af94e75ff72",
      "42b852cd19a1017123307b00c2c7dd3f", "796f3ffb9d7be7b4af1b5b2354f078de",
      "31e728e592eeb4186e9c296cba14431b", "958e9225425973e0efb72b5631bb7567",
      "665f04edc9eb7ecda8b6b3815b2ce70d", "f6c7bdb0f545b6bc21c8626b3ad281d9",
      "2a99d2843237b4648e41cd97d6dc92fa", "1b2be5dd157056fb2be2f201fbc17c32",
      "3f2ed4d6c80df46333e8ed93bf58faf3", "864de7e1d6c9b05b6461dd425911170a",
      "5b1c0ce6558380162e3aa5ab652694b2", "c7c612a7ad16dd9b79c37c08e1595eb5",
      "f0b71f665cfb75dc9f39f1e4e9c3ddd6", "f5066ab69b751ea7b5cd37cb94fe4122",
      "7e7d7675bf768658b9cfe0b70e969475", "3993328e0d01129f2e35ca5462c1614b",
      "bcb804c07f4affa8b33950f9b3942008", "84f597dfb56e9db5953b42eaa2c22c88",
      "c8c1c7d314c5b0c0890ca8e4c91ce055", "c26e0a0e90a969d762edcab770bed3b7",
      "e517967d2cf4f1b0fff09d334475e2ae", "bc760a328a0a4b2d75593667adfa2a0e",
      "b6239fdeeccc462640047cb2e2c2be96", "bc01f6a232ef9f0d9e57301779edd67f",
      "cf6e8c1823c5498fa5589db40406a6ad", "2a9a4bd0bd84f0b85225a5b30f5eaa16",
      "56f7bb2265dbd8a563bb269aa527c8a3", "fcbed0f0350be5a1384f95f8090d262e",
      "f3ecf2e5747ebff65ac78ecbe7cc5e6a", "1d57d1371ad2f5f320cc4de789665f7c",
      "e9f400fee64673b0f6313400fe449135", "5dfdc4a8376740011c777df46418b5d2",
      "a4eb2c077300c0d8eeda028c9db3a63a", "90551259280c2b2150f018304204f072",
      "4cbcd76496fc5b841cd164b6067b9c0b", "895964acc7b7e7d084de2266421c351b",
      "af2e05159d369d0e3b72707f242b2845", "c7d393cef751950df3b9ed8056a9ffce",
      "788541c0807aed47b863d47e5912555d", "163a06512f48c1b0f2535c8c50815bcc",
      "dc5e723bab9fbfd7074a62e05b6b3c2b", "bf91200ce1bf97b4642a601adc13d700",
      "d93fcefa6b9004baaab76d436e7ac931", "e89a2111caecc6bcf5f2b42ea0167ab4",
      "e04a058df9b87878ca97edc1c42e76e1", "5d1f60876147edd6ed29d1fb50172464",
      "655fb228aa410fd244c58c87fe510bec", "639a8a0a8f62d628136f5a97b3728b69",
      "5b60f2428b092a502d6471fa09befd7f", "40601555ac945b4d37d3434b6e5619be",
      "02be23bf1f89d5f5af02a39b98f96142", "9347a45bd54d28d8105f8183996b3505",
      "d8429cc7b0b388981861a0fdd40289f0", "c4b7fab3b044486f663e160c07805e0a",
      "f5f5d513b1f1c13d0abc70fc18afea48", "f236795ea30f1b8761b268734a245ba1",
      "c7b7452ea8247a3a40248278d08953d5", "ddd6ba3c5ec56cc7a0b0161ae67001fa",
      "94675749f2db46a8ade6f2f211db9a32", "3d165364ff96a5ef39e67a53fe3ed3be",
      "3d1d66a9401fd7e78050724ca1fa0419",
  };
  return kDigest[id];
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

struct MaskBlendTestParam {
  MaskBlendTestParam(int width, int height, int subsampling_x,
                     int subsampling_y, bool is_inter_intra,
                     bool is_wedge_inter_intra)
      : width(width),
        height(height),
        subsampling_x(subsampling_x),
        subsampling_y(subsampling_y),
        is_inter_intra(is_inter_intra),
        is_wedge_inter_intra(is_wedge_inter_intra) {}
  int width;
  int height;
  int subsampling_x;
  int subsampling_y;
  bool is_inter_intra;
  bool is_wedge_inter_intra;
};

std::ostream& operator<<(std::ostream& os, const MaskBlendTestParam& param) {
  return os << "BlockSize" << param.width << "x" << param.height
            << ", subsampling(x/y): " << param.subsampling_x << "/"
            << param.subsampling_y
            << ", is_inter_intra: " << param.is_inter_intra
            << ", is_wedge_inter_intra: " << param.is_wedge_inter_intra;
}

template <int bitdepth, typename Pixel>
class MaskBlendTest : public ::testing::TestWithParam<MaskBlendTestParam>,
                      public test_utils::MaxAlignedAllocable {
 public:
  MaskBlendTest() = default;
  ~MaskBlendTest() override = default;

  void SetUp() override {
    test_utils::ResetDspTable(bitdepth);
    MaskBlendInit_C();
    const dsp::Dsp* const dsp = dsp::GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const absl::string_view test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
    } else if (absl::StartsWith(test_case, "NEON/")) {
      MaskBlendInit_NEON();
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        MaskBlendInit_SSE4_1();
      }
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
    func_ = (param_.is_inter_intra && !param_.is_wedge_inter_intra)
                ? dsp->mask_blend[0][param_.is_inter_intra]
                : dsp->mask_blend[param_.subsampling_x + param_.subsampling_y]
                                 [param_.is_inter_intra];
    func_8bpp_ = dsp->inter_intra_mask_blend_8bpp[param_.is_wedge_inter_intra
                                                      ? param_.subsampling_x +
                                                            param_.subsampling_y
                                                      : 0];
  }

 protected:
  int GetDigestIdOffset() const {
    // id is for retrieving the corresponding digest from the lookup table given
    // the set of input parameters. id can be figured out by its width, height
    // and an offset (id_offset).
    // For example, in kMaskBlendTestParam, this set of parameters
    // (8, 8, 0, 0, false, false) corresponds to the first entry in the
    // digest lookup table, where id == 0.
    // (8, 8, 1, 0, false, false) corresponds to id == 13.
    // (8, 8, 1, 1, false, false) corresponds to id == 26.
    // (8, 8, 0, 0, true, false) corresponds to id == 39.
    // Id_offset denotes offset for different modes (is_inter_intra,
    // is_wedge_inter_intra). Width and height help to figure out id:
    // width = 8, height = 8, id = id_offset + log2(8) - 3.
    // width = 8, height = 16, id = id_offset + log2(min(width, height) - 3 + 1.
    // ...
    if (!param_.is_inter_intra && !param_.is_wedge_inter_intra) {
      return param_.subsampling_x * 13 + param_.subsampling_y * 13;
    }
    if (param_.is_inter_intra && !param_.is_wedge_inter_intra) {
      return 39 + param_.subsampling_x * 7 + param_.subsampling_y * 7;
    }
    if (param_.is_inter_intra && param_.is_wedge_inter_intra) {
      return 60 + param_.subsampling_x * 7 + param_.subsampling_y * 7;
    }
    return 0;
  }

  int GetDigestId() const {
    int id = GetDigestIdOffset();
    if (param_.width == param_.height) {
      return id + 3 * (FloorLog2(param_.width) - 3);
    }
    if (param_.width < param_.height) {
      return id + 1 + 3 * (FloorLog2(param_.width) - 3);
    }
    return id + 2 + 3 * (FloorLog2(param_.height) - 3);
  }

  void Test(const char* digest, int num_runs);

 private:
  static constexpr int kStride = kMaxSuperBlockSizeInPixels;
  static constexpr int kDestStride = kMaxSuperBlockSizeInPixels * sizeof(Pixel);
  const MaskBlendTestParam param_ = GetParam();
  alignas(kMaxAlignment) uint16_t
      source1_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels] = {};
  uint8_t source1_8bpp_[kMaxSuperBlockSizeInPixels *
                        kMaxSuperBlockSizeInPixels] = {};
  alignas(kMaxAlignment) uint16_t
      source2_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels] = {};
  uint8_t source2_8bpp_[kMaxSuperBlockSizeInPixels *
                        kMaxSuperBlockSizeInPixels] = {};
  uint8_t source2_8bpp_cache_[kMaxSuperBlockSizeInPixels *
                              kMaxSuperBlockSizeInPixels] = {};
  uint8_t mask_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels];
  uint8_t dest_[sizeof(Pixel) * kMaxSuperBlockSizeInPixels *
                kMaxSuperBlockSizeInPixels] = {};
  dsp::MaskBlendFunc func_;
  dsp::InterIntraMaskBlendFunc8bpp func_8bpp_;
};

template <int bitdepth, typename Pixel>
void MaskBlendTest<bitdepth, Pixel>::Test(const char* const digest,
                                          const int num_runs) {
  if (func_ == nullptr && func_8bpp_ == nullptr) return;
  const int width = param_.width >> param_.subsampling_x;
  const int height = param_.height >> param_.subsampling_y;

  // Add id offset to seed just to add more randomness to input blocks.
  // If we use the same seed for different block sizes, the generated input
  // blocks are repeated. For example, if input size is 8x8, the generated
  // block is exactly the upper left half of the generated 16x16 block.
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed() +
                             GetDigestIdOffset());
  uint16_t* src_1 = source1_;
  uint8_t* src_1_8bpp = source1_8bpp_;
  uint16_t* src_2 = source2_;
  uint8_t* src_2_8bpp = source2_8bpp_;
  const ptrdiff_t src_2_stride = param_.is_inter_intra ? kStride : width;
  uint8_t* mask_row = mask_;
  uint8_t round_bits[2] = {3, 7};
  if (param_.is_inter_intra) round_bits[1] = 11;
  const int inter_post_round_bits =
      2 * kFilterBits - round_bits[0] - round_bits[1];
  const int range_mask = (1 << (bitdepth + inter_post_round_bits)) - 1;
  const int round_offset = (bitdepth == 8) ? 0 : kCompoundOffset;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      src_1[x] = rnd.Rand16() & range_mask;
      src_2[x] = rnd.Rand16() & range_mask;
      if (param_.is_inter_intra && bitdepth == 8) {
        src_1_8bpp[x] = src_1[x];
        src_2_8bpp[x] = src_2[x];
      }
      if (!param_.is_inter_intra) {
        src_1[x] += round_offset;
        src_2[x] += round_offset;
      }
    }
    src_1 += width;
    src_1_8bpp += width;
    src_2 += src_2_stride;
    src_2_8bpp += src_2_stride;
  }
  // Mask should be setup regardless of subsampling.
  for (int y = 0; y < param_.height; ++y) {
    for (int x = 0; x < param_.width; ++x) {
      mask_row[x] = rnd.Rand8() & 63;
      mask_row[x] += rnd.Rand8() & 1;  // Range of mask is [0, 64].
    }
    mask_row += kStride;
  }

  absl::Duration elapsed_time;
  for (int i = 0; i < num_runs; ++i) {
    const absl::Time start = absl::Now();
    if (param_.is_inter_intra && bitdepth == 8) {
      ASSERT_EQ(func_, nullptr);
      static_assert(sizeof(source2_8bpp_cache_) == sizeof(source2_8bpp_), "");
      // source2_8bpp_ is modified in the call.
      memcpy(source2_8bpp_cache_, source2_8bpp_, sizeof(source2_8bpp_));
      func_8bpp_(source1_8bpp_, source2_8bpp_, src_2_stride, mask_, kStride,
                 width, height);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          dest_[y * kDestStride + x] = source2_8bpp_[y * src_2_stride + x];
        }
      }
      memcpy(source2_8bpp_, source2_8bpp_cache_, sizeof(source2_8bpp_));
    } else {
      if (bitdepth != 8) {
        ASSERT_EQ(func_8bpp_, nullptr);
      }
      func_(source1_, source2_, src_2_stride, mask_, kStride, width, height,
            dest_, kDestStride);
    }
    elapsed_time += absl::Now() - start;
  }

  test_utils::CheckMd5Digest(
      "MaskBlend",
      absl::StrFormat("%dx%d", param_.width, param_.height).c_str(), digest,
      dest_, sizeof(dest_), elapsed_time);
}

const MaskBlendTestParam kMaskBlendTestParam[] = {
    // is_inter_intra = false, is_wedge_inter_intra = false.
    // block size range is from 8x8 to 128x128.
    MaskBlendTestParam(8, 8, 0, 0, false, false),
    MaskBlendTestParam(8, 16, 0, 0, false, false),
    MaskBlendTestParam(16, 8, 0, 0, false, false),
    MaskBlendTestParam(16, 16, 0, 0, false, false),
    MaskBlendTestParam(16, 32, 0, 0, false, false),
    MaskBlendTestParam(32, 16, 0, 0, false, false),
    MaskBlendTestParam(32, 32, 0, 0, false, false),
    MaskBlendTestParam(32, 64, 0, 0, false, false),
    MaskBlendTestParam(64, 32, 0, 0, false, false),
    MaskBlendTestParam(64, 64, 0, 0, false, false),
    MaskBlendTestParam(64, 128, 0, 0, false, false),
    MaskBlendTestParam(128, 64, 0, 0, false, false),
    MaskBlendTestParam(128, 128, 0, 0, false, false),
    MaskBlendTestParam(8, 8, 1, 0, false, false),
    MaskBlendTestParam(8, 16, 1, 0, false, false),
    MaskBlendTestParam(16, 8, 1, 0, false, false),
    MaskBlendTestParam(16, 16, 1, 0, false, false),
    MaskBlendTestParam(16, 32, 1, 0, false, false),
    MaskBlendTestParam(32, 16, 1, 0, false, false),
    MaskBlendTestParam(32, 32, 1, 0, false, false),
    MaskBlendTestParam(32, 64, 1, 0, false, false),
    MaskBlendTestParam(64, 32, 1, 0, false, false),
    MaskBlendTestParam(64, 64, 1, 0, false, false),
    MaskBlendTestParam(64, 128, 1, 0, false, false),
    MaskBlendTestParam(128, 64, 1, 0, false, false),
    MaskBlendTestParam(128, 128, 1, 0, false, false),
    MaskBlendTestParam(8, 8, 1, 1, false, false),
    MaskBlendTestParam(8, 16, 1, 1, false, false),
    MaskBlendTestParam(16, 8, 1, 1, false, false),
    MaskBlendTestParam(16, 16, 1, 1, false, false),
    MaskBlendTestParam(16, 32, 1, 1, false, false),
    MaskBlendTestParam(32, 16, 1, 1, false, false),
    MaskBlendTestParam(32, 32, 1, 1, false, false),
    MaskBlendTestParam(32, 64, 1, 1, false, false),
    MaskBlendTestParam(64, 32, 1, 1, false, false),
    MaskBlendTestParam(64, 64, 1, 1, false, false),
    MaskBlendTestParam(64, 128, 1, 1, false, false),
    MaskBlendTestParam(128, 64, 1, 1, false, false),
    MaskBlendTestParam(128, 128, 1, 1, false, false),
    // is_inter_intra = true, is_wedge_inter_intra = false.
    // block size range is from 8x8 to 32x32.
    MaskBlendTestParam(8, 8, 0, 0, true, false),
    MaskBlendTestParam(8, 16, 0, 0, true, false),
    MaskBlendTestParam(16, 8, 0, 0, true, false),
    MaskBlendTestParam(16, 16, 0, 0, true, false),
    MaskBlendTestParam(16, 32, 0, 0, true, false),
    MaskBlendTestParam(32, 16, 0, 0, true, false),
    MaskBlendTestParam(32, 32, 0, 0, true, false),
    MaskBlendTestParam(8, 8, 1, 0, true, false),
    MaskBlendTestParam(8, 16, 1, 0, true, false),
    MaskBlendTestParam(16, 8, 1, 0, true, false),
    MaskBlendTestParam(16, 16, 1, 0, true, false),
    MaskBlendTestParam(16, 32, 1, 0, true, false),
    MaskBlendTestParam(32, 16, 1, 0, true, false),
    MaskBlendTestParam(32, 32, 1, 0, true, false),
    MaskBlendTestParam(8, 8, 1, 1, true, false),
    MaskBlendTestParam(8, 16, 1, 1, true, false),
    MaskBlendTestParam(16, 8, 1, 1, true, false),
    MaskBlendTestParam(16, 16, 1, 1, true, false),
    MaskBlendTestParam(16, 32, 1, 1, true, false),
    MaskBlendTestParam(32, 16, 1, 1, true, false),
    MaskBlendTestParam(32, 32, 1, 1, true, false),
    // is_inter_intra = true, is_wedge_inter_intra = true.
    // block size range is from 8x8 to 32x32.
    MaskBlendTestParam(8, 8, 0, 0, true, true),
    MaskBlendTestParam(8, 16, 0, 0, true, true),
    MaskBlendTestParam(16, 8, 0, 0, true, true),
    MaskBlendTestParam(16, 16, 0, 0, true, true),
    MaskBlendTestParam(16, 32, 0, 0, true, true),
    MaskBlendTestParam(32, 16, 0, 0, true, true),
    MaskBlendTestParam(32, 32, 0, 0, true, true),
    MaskBlendTestParam(8, 8, 1, 0, true, true),
    MaskBlendTestParam(8, 16, 1, 0, true, true),
    MaskBlendTestParam(16, 8, 1, 0, true, true),
    MaskBlendTestParam(16, 16, 1, 0, true, true),
    MaskBlendTestParam(16, 32, 1, 0, true, true),
    MaskBlendTestParam(32, 16, 1, 0, true, true),
    MaskBlendTestParam(32, 32, 1, 0, true, true),
    MaskBlendTestParam(8, 8, 1, 1, true, true),
    MaskBlendTestParam(8, 16, 1, 1, true, true),
    MaskBlendTestParam(16, 8, 1, 1, true, true),
    MaskBlendTestParam(16, 16, 1, 1, true, true),
    MaskBlendTestParam(16, 32, 1, 1, true, true),
    MaskBlendTestParam(32, 16, 1, 1, true, true),
    MaskBlendTestParam(32, 32, 1, 1, true, true),
};

using MaskBlendTest8bpp = MaskBlendTest<8, uint8_t>;

TEST_P(MaskBlendTest8bpp, Blending) { Test(GetDigest8bpp(GetDigestId()), 1); }

TEST_P(MaskBlendTest8bpp, DISABLED_Speed) {
  Test(GetDigest8bpp(GetDigestId()), kNumSpeedTests);
}

INSTANTIATE_TEST_SUITE_P(C, MaskBlendTest8bpp,
                         ::testing::ValuesIn(kMaskBlendTestParam));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, MaskBlendTest8bpp,
                         ::testing::ValuesIn(kMaskBlendTestParam));
#endif

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, MaskBlendTest8bpp,
                         ::testing::ValuesIn(kMaskBlendTestParam));
#endif

#if LIBGAV1_MAX_BITDEPTH >= 10
using MaskBlendTest10bpp = MaskBlendTest<10, uint16_t>;

TEST_P(MaskBlendTest10bpp, Blending) { Test(GetDigest10bpp(GetDigestId()), 1); }

TEST_P(MaskBlendTest10bpp, DISABLED_Speed) {
  Test(GetDigest10bpp(GetDigestId()), kNumSpeedTests);
}

INSTANTIATE_TEST_SUITE_P(C, MaskBlendTest10bpp,
                         ::testing::ValuesIn(kMaskBlendTestParam));

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, MaskBlendTest10bpp,
                         ::testing::ValuesIn(kMaskBlendTestParam));
#endif
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
