// Copyright 2021 The libgav1 Authors
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

#include "src/dsp/cdef.h"

#include <cstdint>
#include <cstring>
#include <ostream>

#include "absl/strings/match.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/cpu.h"
#include "src/utils/memory.h"
#include "tests/third_party/libvpx/acm_random.h"
#include "tests/third_party/libvpx/md5_helper.h"
#include "tests/utils.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr char kCdef[] = "Cdef";
constexpr char kCdefDirectionName[] = "Cdef Direction";
constexpr char kCdefFilterName[] = "Cdef Filtering";
constexpr int kTestBufferStride = 8;
constexpr int kTestBufferSize = 64;
constexpr int kSourceStride = kMaxSuperBlockSizeInPixels + 2 * 8;
constexpr int kSourceBufferSize =
    (kMaxSuperBlockSizeInPixels + 2 * 3) * kSourceStride;
constexpr int kNumSpeedTests = 5000;

const char* GetDirectionDigest(const int bitdepth, const int num_runs) {
  static const char* const kDigest[2][2] = {
      {"de78c820a1fec7e81385aa0a615dbf8c", "7bfc543244f932a542691480dc4541b2"},
      {"b54236de5d25e16c0f8678d9784cb85e", "559144cf183f3c69cb0e5d98cbf532ff"}};
  const int bitdepth_index = (bitdepth == 8) ? 0 : 1;
  const int run_index = (num_runs == 1) ? 0 : 1;
  return kDigest[bitdepth_index][run_index];
}

template <int bitdepth, typename Pixel>
class CdefDirectionTest : public testing::TestWithParam<int> {
 public:
  CdefDirectionTest() = default;
  CdefDirectionTest(const CdefDirectionTest&) = delete;
  CdefDirectionTest& operator=(const CdefDirectionTest&) = delete;
  ~CdefDirectionTest() override = default;

 protected:
  void SetUp() override {
    test_utils::ResetDspTable(bitdepth);
    CdefInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    base_cdef_direction_ = nullptr;
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      CdefInit_SSE4_1();
    } else if (absl::StartsWith(test_case, "AVX2/")) {
      if ((GetCpuInfo() & kAVX2) != 0) {
        CdefInit_AVX2();
      }
    } else if (absl::StartsWith(test_case, "NEON/")) {
      CdefInit_NEON();
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
    cur_cdef_direction_ = dsp->cdef_direction;
  }

  void TestRandomValues(int num_runs);

  Pixel buffer_[kTestBufferSize];
  int strength_;
  int size_;

  CdefDirectionFunc base_cdef_direction_;
  CdefDirectionFunc cur_cdef_direction_;
};

template <int bitdepth, typename Pixel>
void CdefDirectionTest<bitdepth, Pixel>::TestRandomValues(int num_runs) {
  if (cur_cdef_direction_ == nullptr) return;
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  absl::Duration elapsed_time;
  libvpx_test::MD5 actual_digest;
  for (int num_tests = 0; num_tests < num_runs; ++num_tests) {
    for (int level = 0; level < (1 << bitdepth); level += 1 + (bitdepth - 8)) {
      for (int bits = 0; bits <= bitdepth; ++bits) {
        for (auto& pixel : buffer_) {
          pixel = Clip3((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                        (1 << bitdepth) - 1);
        }
        int output[2] = {};
        const absl::Time start = absl::Now();
        cur_cdef_direction_(buffer_, kTestBufferStride * sizeof(Pixel),
                            reinterpret_cast<uint8_t*>(&output[0]), &output[1]);
        elapsed_time += absl::Now() - start;
        actual_digest.Add(reinterpret_cast<const uint8_t*>(output),
                          sizeof(output));
      }
    }
  }
  test_utils::CheckMd5Digest(kCdef, kCdefDirectionName,
                             GetDirectionDigest(bitdepth, num_runs),
                             actual_digest.Get(), elapsed_time);
}

using CdefDirectionTest8bpp = CdefDirectionTest<8, uint8_t>;

TEST_P(CdefDirectionTest8bpp, Correctness) { TestRandomValues(1); }

TEST_P(CdefDirectionTest8bpp, DISABLED_Speed) {
  TestRandomValues(kNumSpeedTests / 100);
}

INSTANTIATE_TEST_SUITE_P(C, CdefDirectionTest8bpp, testing::Values(0));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, CdefDirectionTest8bpp, testing::Values(0));
#endif

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, CdefDirectionTest8bpp, testing::Values(0));
#endif

#if LIBGAV1_ENABLE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, CdefDirectionTest8bpp, testing::Values(0));
#endif  // LIBGAV1_ENABLE_AVX2

#if LIBGAV1_MAX_BITDEPTH >= 10
using CdefDirectionTest10bpp = CdefDirectionTest<10, uint16_t>;

TEST_P(CdefDirectionTest10bpp, Correctness) { TestRandomValues(1); }

TEST_P(CdefDirectionTest10bpp, DISABLED_Speed) {
  TestRandomValues(kNumSpeedTests / 100);
}

INSTANTIATE_TEST_SUITE_P(C, CdefDirectionTest10bpp, testing::Values(0));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, CdefDirectionTest10bpp, testing::Values(0));
#endif
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

const char* GetDigest8bpp(int id) {
  static const char* const kDigest[] = {
      "b6fe1a1f5bbb23e35197160ce57d90bd", "8aed39871b19184f1d381b145779bc33",
      "82653dd66072e8ebd967083a0413ab03", "421c048396bc66ffaa6aafa016c7bc54",
      "1f70ba51091e8c6034c3f0974af241c3", "8f700997452a24091136ca58890a5be4",
      "9deaaf07db25ca1d96ea8762925372d3", "7edadd9ad058be518430e64f78fe34a2",
      "862362a654edb2562609895395eb69cd", "3b4dae4d353b75f652ce67f96b2fd718",
      "65c51f49e4fd848d9fef23a346702b17", "f93b3fa86764e53e4c206ef01d5ee9db",
      "202e36551bc147c30b76ae359d5f7646", "3de677a2b6fe4aa6fc29a5e5f2d63063",
      "ab860362809e878f7b47dacc6087bce3", "c0d991affc8aeb45d91ae36e7b3d77d8",
      "27f19fffabfb79104b4be3c272723f62", "a54b981f562e2cf10a4fb037d0181e2d",
      "9a65933d02867a1e8fc1f29097d4d0db", "c068b21d232145c61db8ef9298447bfa",
      "8db1948c23648372509e4f3577e8eaa0", "c08a3b192ab0a47abe22f7f0ae78a5d7",
      "4ff9bd4ae06f2cc2d2660df41cf1baca", "a0a634e48c55a2ca340cf5cac7f74cb6",
      "f9f631985b42214f8b059c8f119d4401", "5fb136073300a45d74145649473970da",
      "33624aab8ba0264657fa9304dbdcf72c", "e6a15775d451a3c4803a7c0604deb0ea",
      "4c28b63022cdc5ea0e49b492c187d53d", "c5fa9792ee292d29c5a864e376ddacc0",
      "fcdf7319978b64f03ca3b9d4d83a0c2a", "394931c89bd5065308b0633d12370b19",
      "9e702d68000c1b02759001e9a8876df2", "c844919f0114e83960dd329b1aa7146f",
      "499248c675884db3ef57018d0a0868b5", "4a9041ed183f9add717e5ddcdb280799",
  };
  return kDigest[id];
}

#if LIBGAV1_MAX_BITDEPTH >= 10
const char* GetDigest10bpp(int id) {
  static const char* const kDigest[] = {
      "0a9630b39974850998db653b07e09ab4", "97a924661d931b23ee57893da617ae70",
      "0d79516b9a491ce5112eb00bbae5eb80", "d5801fd96029a7509cf66dde61e8e2d8",
      "5bf5c0ea5a85e9b6c1e6991619c34ebc", "e2f1c08a8b3cd93b3a85511493a0ee31",
      "18910f422e386c71ffde8680176d61c0", "3255afe8b3db5be4c17299420ae9b4b3",
      "ccac34de92891d4ef25820737e7a4f06", "5c2109c4142867c15bc6bb81e19b8058",
      "86e8300e2ad292bfce95185530ef06c8", "21c06ed6d62b8fbef1363cd177386cd0",
      "fd6687987dbff6f15210c2cc61570daa", "7cb246cb65a9cf9b2f829ab086f7c45a",
      "3a38dc3c89f7e400383b1b7ce3e73008", "7b23b520e41ad510b9608b47f9c5f87e",
      "f9ca24b57fc06d7b8dc4151bbc4d2840", "070ef8fa64dcdc45701428ee6ef0ca79",
      "0e7e3ca3cf8546972d01fc262b2b9cfb", "9ac81b7cf93173f33d195927b0a3685a",
      "1f964b6959774651a79d961e5a2a6a56", "64d5f88995a918a85df317d4240f0862",
      "55c94ec09facda30fac677d205beb708", "2c010b256f4dabf42ef78bf5a3851b2c",
      "c7d18d0e287fa8658b94131603e378db", "4f7696fe2c8dbedd0c8e8a53b9dec0fc",
      "b3483dc32665a4bb0606d78dfb3d285c", "0bcb4acd4090f5798c2d260df73b2c46",
      "4f574c782f3b28fb9c85cdb70dfcb46a", "14bd700a88be0107e9ef2fe54f75cee6",
      "5d3b2698c9ffa4a6aed45a9adbddb8bf", "eff870414f80897cf8958ebeea84f0a6",
      "e042843275f82271a9f540bc3e4ef35c", "26e3ff3d661dac25861a0f5bab522340",
      "239844e66b07796003f9315166b9e29e", "44b8e6884215a1793cc7f8f7ce40bcee",
  };
  return kDigest[id];
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

struct CdefTestParam {
  CdefTestParam(int subsampling_x, int subsampling_y, int rows4x4,
                int columns4x4)
      : subsampling_x(subsampling_x),
        subsampling_y(subsampling_y),
        rows4x4(rows4x4),
        columns4x4(columns4x4) {}
  int subsampling_x;
  int subsampling_y;
  int rows4x4;
  int columns4x4;
};

std::ostream& operator<<(std::ostream& os, const CdefTestParam& param) {
  return os << "subsampling(x/y): " << param.subsampling_x << "/"
            << param.subsampling_y << ", (rows,columns)4x4: " << param.rows4x4
            << ", " << param.columns4x4;
}

// TODO(b/154245961): rework the parameters for this test to match
// CdefFilteringFuncs. It should cover 4x4, 8x4, 8x8 blocks and
// primary/secondary strength combinations for both Y and UV.
template <int bitdepth, typename Pixel>
class CdefFilteringTest : public testing::TestWithParam<CdefTestParam> {
 public:
  CdefFilteringTest() = default;
  CdefFilteringTest(const CdefFilteringTest&) = delete;
  CdefFilteringTest& operator=(const CdefFilteringTest&) = delete;
  ~CdefFilteringTest() override = default;

 protected:
  void SetUp() override {
    test_utils::ResetDspTable(bitdepth);
    CdefInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
    } else if (absl::StartsWith(test_case, "NEON/")) {
      CdefInit_NEON();
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      CdefInit_SSE4_1();
    } else if (absl::StartsWith(test_case, "AVX2/")) {
      if ((GetCpuInfo() & kAVX2) != 0) {
        CdefInit_AVX2();
      }
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
    memcpy(cur_cdef_filter_, dsp->cdef_filters, sizeof(cur_cdef_filter_));
  }

  void TestRandomValues(int num_runs);

  uint16_t source_[kSourceBufferSize];
  Pixel dest_[kMaxPlanes][kTestBufferSize];
  int primary_strength_;
  int secondary_strength_;
  int damping_;
  int direction_;
  CdefTestParam param_ = GetParam();

  CdefFilteringFuncs cur_cdef_filter_;
};

template <int bitdepth, typename Pixel>
void CdefFilteringTest<bitdepth, Pixel>::TestRandomValues(int num_runs) {
  const int id = (static_cast<int>(param_.rows4x4 < 4) +
                  static_cast<int>(param_.rows4x4 < 2)) *
                     3 +
                 param_.subsampling_x * 9 + param_.subsampling_y * 18;
  absl::Duration elapsed_time;
  for (int num_tests = 0; num_tests < num_runs; ++num_tests) {
    for (int plane = kPlaneY; plane < kMaxPlanes; ++plane) {
      const int subsampling_x = (plane == kPlaneY) ? 0 : param_.subsampling_x;
      const int subsampling_y = (plane == kPlaneY) ? 0 : param_.subsampling_y;
      const int block_width = 8 >> subsampling_x;
      const int block_height = 8 >> subsampling_y;
      libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed() +
                                 id + plane);
      const int offset = 2 * kSourceStride + 2;
      // Fill boundaries with a large value such that cdef does not take them
      // into calculation.
      const int plane_width = MultiplyBy4(param_.columns4x4) >> subsampling_x;
      const int plane_height = MultiplyBy4(param_.rows4x4) >> subsampling_y;
      for (int y = 0; y < plane_height; ++y) {
        for (int x = 0; x < plane_width; ++x) {
          source_[y * kSourceStride + x + offset] =
              rnd.Rand16() & ((1 << bitdepth) - 1);
        }
      }
      for (int y = 0; y < 2; ++y) {
        Memset(&source_[y * kSourceStride], kCdefLargeValue, kSourceStride);
        Memset(&source_[(y + plane_height + 2) * kSourceStride],
               kCdefLargeValue, kSourceStride);
      }
      for (int y = 0; y < plane_height; ++y) {
        Memset(&source_[y * kSourceStride + offset - 2], kCdefLargeValue, 2);
        Memset(&source_[y * kSourceStride + offset + plane_width],
               kCdefLargeValue, 2);
      }
      do {
        int strength = rnd.Rand16() & 15;
        if (strength == 3) ++strength;
        primary_strength_ = strength << (bitdepth - 8);
      } while (primary_strength_ == 0);
      do {
        int strength = rnd.Rand16() & 3;
        if (strength == 3) ++strength;
        secondary_strength_ = strength << (bitdepth - 8);
      } while (secondary_strength_ == 0);
      damping_ = (rnd.Rand16() & 3) + 3;
      direction_ = (rnd.Rand16() & 7);

      memset(dest_[plane], 0, sizeof(dest_[plane]));
      const absl::Time start = absl::Now();
      const int width_index = block_width >> 3;
      if (cur_cdef_filter_[width_index][0] == nullptr) return;
      cur_cdef_filter_[width_index][0](
          source_ + offset, kSourceStride, block_height, primary_strength_,
          secondary_strength_, damping_, direction_, dest_[plane],
          kTestBufferStride * sizeof(dest_[0][0]));
      elapsed_time += absl::Now() - start;
    }
  }

  for (int plane = kPlaneY; plane < kMaxPlanes; ++plane) {
    if (bitdepth == 8) {
      test_utils::CheckMd5Digest(kCdef, kCdefFilterName,
                                 GetDigest8bpp(id + plane),
                                 reinterpret_cast<uint8_t*>(dest_[plane]),
                                 sizeof(dest_[plane]), elapsed_time);
#if LIBGAV1_MAX_BITDEPTH >= 10
    } else {
      test_utils::CheckMd5Digest(kCdef, kCdefFilterName,
                                 GetDigest10bpp(id + plane),
                                 reinterpret_cast<uint8_t*>(dest_[plane]),
                                 sizeof(dest_[plane]), elapsed_time);
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
    }
  }
}

// Do not test single blocks with any subsampling. 2xH and Wx2 blocks are not
// supported.
const CdefTestParam cdef_test_param[] = {
    CdefTestParam(0, 0, 4, 4), CdefTestParam(0, 0, 2, 2),
    CdefTestParam(1, 0, 4, 4), CdefTestParam(1, 0, 2, 2),
    CdefTestParam(0, 1, 4, 4), CdefTestParam(0, 1, 2, 2),
    CdefTestParam(1, 1, 4, 4), CdefTestParam(1, 1, 2, 2),
};

using CdefFilteringTest8bpp = CdefFilteringTest<8, uint8_t>;

TEST_P(CdefFilteringTest8bpp, Correctness) { TestRandomValues(1); }

TEST_P(CdefFilteringTest8bpp, DISABLED_Speed) {
  TestRandomValues(kNumSpeedTests);
}

INSTANTIATE_TEST_SUITE_P(C, CdefFilteringTest8bpp,
                         testing::ValuesIn(cdef_test_param));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, CdefFilteringTest8bpp,
                         testing::ValuesIn(cdef_test_param));
#endif

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE41, CdefFilteringTest8bpp,
                         testing::ValuesIn(cdef_test_param));
#endif

#if LIBGAV1_ENABLE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, CdefFilteringTest8bpp,
                         testing::ValuesIn(cdef_test_param));
#endif  // LIBGAV1_ENABLE_AVX2

#if LIBGAV1_MAX_BITDEPTH >= 10
using CdefFilteringTest10bpp = CdefFilteringTest<10, uint16_t>;

TEST_P(CdefFilteringTest10bpp, Correctness) { TestRandomValues(1); }

TEST_P(CdefFilteringTest10bpp, DISABLED_Speed) {
  TestRandomValues(kNumSpeedTests);
}

INSTANTIATE_TEST_SUITE_P(C, CdefFilteringTest10bpp,
                         testing::ValuesIn(cdef_test_param));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, CdefFilteringTest10bpp,
                         testing::ValuesIn(cdef_test_param));
#endif
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
