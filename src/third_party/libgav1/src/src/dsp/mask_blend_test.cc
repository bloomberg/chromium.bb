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

constexpr int kNumSpeedTests = 50000;
// mask_blend is applied to compound prediction values when is_inter_intra is
// false. This implies a range far exceeding that of pixel values. The ranges
// include kCompoundOffset in 10bpp and 12bpp.
// see: src/dsp/convolve.cc & src/dsp/warp.cc.
constexpr int kCompoundPredictionRange[3][2] = {
    // 8bpp
    {-5132, 9212},
    // 10bpp
    {3988, 61532},
    // 12bpp
    {3974, 61559},
};

const char* GetDigest8bpp(int id) {
  static const char* const kDigest[] = {
      "4b70d5ef5ac7554b4b2660a4abe14a41", "64adb36f07e4a2c4ea4f05cfd715ff58",
      "c490478208374a43765900ef7115c264", "b98f222eb70ef8589da2d6c839ca22b8",
      "54752ca05f67b5af571bc311aa4e3de3", "344b2dab7accd8bd0a255bee16207336",
      "0b2f6f755d1547eea7e0172f8133ea01", "310dc6364fdacba186c01f0e8ac4fcb7",
      "b0c9f08b73d9e5c16eaf5abdbca1fdc0", "eaad805999d949fa1e1bbbb63b4b7827",
      "6eb2a80d212df89403efb50db7a81b08", "c30730aa799dba78a2ebd3f729af82c7",
      "4346c2860b23f0072b6b288f14c1df36", "8f8dd3eeed74ef115ca8a2f82ebff0ba",
      "42e8872a81647767636f4c75609e0e2f", "1ff2526547d59557f7bb458249e34527",
      "cd303d685268aebd2919dd468928d0ba", "254fb3ad990f9d408d252c70dd682e27",
      "ba8d99c62853d14855f5d93e9574c97b", "e8ab744348681d6aa1043080efa86fc9",
      "2fa919ca1f54b4336de878ff4015c352", "18e47c9809b909c2bfad08e00dffc635",
      "9a90c843f06f0b662c509c26f5dd5054", "f89c608f884f37b064fc2b49eb2690a9",
      "2448734d948ca6ddeb0ce8038a4ab2cf", "a3e0f86b7a5cb49716a424709c00b5a4",
      "eb84dba768b54da10cded2f932f0aab7", "d6e8fdeb6875b70488f25d7f7ed9423f",
      "1ca0822febce19c02ddc42a7b3331257", "a9259bb9b87ad002619eb47b907d7226",
      "6408c5f327f1a9a390fb0046d4bc112b", "dba612489f87d00a82f2735fbcb98dcc",
      "e8626a97699fbd247d6358ad5f766bee", "5e638a6897d7a2950f3512f871fa19e6",
      "45a58708939779413f8e0e1de2ee5e6f", "079ae4682d398f0a7e4b66059589586d",
      "6a06e617308409f9181b59bdd4f63d83", "b05ade2c1a572fc5fcca92b4163d9afb",
      "30e955c3f86111207d5922575602e90a", "af5e6c65ed48a0eb7d509f7036398728",
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
      "1af3cbd1616941b59e6a3f6a417b6312", "1d8b3f4b9d5d2f4ff5be8e81b7243121",
      "53a3a76bf2bcd5761cd15fc739a4f4e1", "7597f69dc19a584280be0d67911db6a6",
      "e1221c172843dc6c1b345bcd370771cc", "2ccbe012ca167114b14c3ba70befa960",
      "0f68632d7e5faddb4554ca430d1df822", "8caa0061a26e142b783951d5abd7bf5d",
      "1cce6acdbd8ca8d2546ba937584730bf", "022913e87a3c1a86aaefe2c2d4f89882",
      "48f8ab636ba15a06731d869b603cbe58", "ba1616c990d224c20de123c3ccf19952",
      "346a797b7cb4de10759e329f8b49e077", "8f4aa102e9b1ac430bdb9ebd4ec4cfca",
      "5886397456b15e504ad55d8e0ce71e0e", "2a78b52ce43dc28606e83521963c00fa",
      "8d3ef5280063337b0df97f91251bb8fc", "81f0ceada000ce40586be828a2045430",
      "edb7b70a473392148bc419a44385326b", "97abe2eecaf9158a0529b234a241a57a",
      "65729d750aa1258e4a7eccef247ac8c2", "78cc995e81188b9e8b29fa58796a3313",
      "a1eb6a8c2f7c77e30e739a1b3b07cc74", "805b0f2f4b9d80f118d800b5ab4f603e",
      "12610c83533f7170149390ba581f70b2", "cba20deed43b49ada3f626c91510995d",
      "ba7ea35410b746fcbcf56c24ccb56d59", "933b2235b9b943984607d87f0bce1067",
      "7ae59015295db8983bc8472429076464", "c18cce63327b367c0a260e9cbf4222b9",
      "7c9672a7dfa964cb3ed3f2b4b443d2b6", "b29bcf1cc5369702e0179db1198db531",
      "412326aff6c89116240b5d3ef63fa5cc", "3d854589fd171e42d118be4627ec5330",
      "9a157e51e39ed314031224f074193791", "c645cdc63d3112f27b90cc9080c6d071",
      "3f360cc336a4ee9a9bd78bde1a6e9eb3", "37b40fa8674d03a7cd66afdee939b9bf",
      "cd6c7b98fe71b533c6a06d6d9122a6d0", "c26e0a0e90a969d762edcab770bed3b7",
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
  using PredType =
      typename std::conditional<bitdepth == 8, int16_t, uint16_t>::type;
  static constexpr int kStride = kMaxSuperBlockSizeInPixels;
  static constexpr int kDestStride = kMaxSuperBlockSizeInPixels * sizeof(Pixel);
  const MaskBlendTestParam param_ = GetParam();
  alignas(kMaxAlignment) PredType
      source1_[kMaxSuperBlockSizeInPixels * kMaxSuperBlockSizeInPixels] = {};
  uint8_t source1_8bpp_[kMaxSuperBlockSizeInPixels *
                        kMaxSuperBlockSizeInPixels] = {};
  alignas(kMaxAlignment) PredType
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
  PredType* src_1 = source1_;
  uint8_t* src_1_8bpp = source1_8bpp_;
  PredType* src_2 = source2_;
  uint8_t* src_2_8bpp = source2_8bpp_;
  const ptrdiff_t src_2_stride = param_.is_inter_intra ? kStride : width;
  uint8_t* mask_row = mask_;
  const int range_mask = (1 << (bitdepth)) - 1;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      src_1[x] = static_cast<PredType>(rnd.Rand16() & range_mask);
      src_2[x] = static_cast<PredType>(rnd.Rand16() & range_mask);
      if (param_.is_inter_intra && bitdepth == 8) {
        src_1_8bpp[x] = src_1[x];
        src_2_8bpp[x] = src_2[x];
      }
      if (!param_.is_inter_intra) {
        // Implies isCompound == true.
        constexpr int bitdepth_index = (bitdepth - 8) >> 1;
        const int min_val = kCompoundPredictionRange[bitdepth_index][0];
        const int max_val = kCompoundPredictionRange[bitdepth_index][1];
        src_1[x] = static_cast<PredType>(rnd(max_val - min_val) + min_val);
        src_2[x] = static_cast<PredType>(rnd(max_val - min_val) + min_val);
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
