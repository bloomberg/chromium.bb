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

#include "tests/utils.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "src/dsp/dsp.h"
#include "src/gav1/decoder_buffer.h"
#include "src/utils/constants.h"
#include "tests/third_party/libvpx/md5_helper.h"

namespace libgav1 {
namespace test_utils {

void ResetDspTable(const int bitdepth) {
  dsp::Dsp* const dsp = dsp_internal::GetWritableDspTable(bitdepth);
  ASSERT_NE(dsp, nullptr);
  memset(dsp, 0, sizeof(dsp::Dsp));
}

std::string GetMd5Sum(const void* bytes, size_t size) {
  libvpx_test::MD5 md5;
  md5.Add(static_cast<const uint8_t*>(bytes), size);
  return md5.Get();
}

template <typename Pixel>
std::string GetMd5Sum(const Pixel* block, int width, int height, int stride) {
  libvpx_test::MD5 md5;
  const Pixel* row = block;
  for (int i = 0; i < height; ++i) {
    md5.Add(reinterpret_cast<const uint8_t*>(row), width * sizeof(Pixel));
    row += stride;
  }
  return md5.Get();
}

template std::string GetMd5Sum(const int8_t* block, int width, int height,
                               int stride);
template std::string GetMd5Sum(const int16_t* block, int width, int height,
                               int stride);

std::string GetMd5Sum(const DecoderBuffer& buffer) {
  libvpx_test::MD5 md5;
  const size_t pixel_size =
      (buffer.bitdepth == 8) ? sizeof(uint8_t) : sizeof(uint16_t);
  for (int plane = kPlaneY; plane < buffer.NumPlanes(); ++plane) {
    const int height = buffer.displayed_height[plane];
    const size_t width = buffer.displayed_width[plane] * pixel_size;
    const int stride = buffer.stride[plane];
    const uint8_t* plane_buffer = buffer.plane[plane];
    for (int row = 0; row < height; ++row) {
      md5.Add(plane_buffer, width);
      plane_buffer += stride;
    }
  }
  return md5.Get();
}

void CheckMd5Digest(const char name[], const char function_name[],
                    const char expected_digest[], const void* data, size_t size,
                    absl::Duration elapsed_time) {
  const std::string digest = test_utils::GetMd5Sum(data, size);
  printf("Mode %s[%31s]: %5d us     MD5: %s\n", name, function_name,
         static_cast<int>(absl::ToInt64Microseconds(elapsed_time)),
         digest.c_str());
  EXPECT_STREQ(expected_digest, digest.c_str());
}

template <typename Pixel>
void CheckMd5Digest(const char name[], const char function_name[],
                    const char expected_digest[], const Pixel* block, int width,
                    int height, int stride, absl::Duration elapsed_time) {
  const std::string digest =
      test_utils::GetMd5Sum(block, width, height, stride);
  printf("Mode %s[%31s]: %5d us     MD5: %s\n", name, function_name,
         static_cast<int>(absl::ToInt64Microseconds(elapsed_time)),
         digest.c_str());
  EXPECT_STREQ(expected_digest, digest.c_str());
}

template void CheckMd5Digest(const char name[], const char function_name[],
                             const char expected_digest[], const int8_t* block,
                             int width, int height, int stride,
                             absl::Duration elapsed_time);
template void CheckMd5Digest(const char name[], const char function_name[],
                             const char expected_digest[], const int16_t* block,
                             int width, int height, int stride,
                             absl::Duration elapsed_time);

void CheckMd5Digest(const char name[], const char function_name[],
                    const char expected_digest[], const char actual_digest[],
                    absl::Duration elapsed_time) {
  printf("Mode %s[%31s]: %5d us     MD5: %s\n", name, function_name,
         static_cast<int>(absl::ToInt64Microseconds(elapsed_time)),
         actual_digest);
  EXPECT_STREQ(expected_digest, actual_digest);
}

}  // namespace test_utils
}  // namespace libgav1
