// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm2pes.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#include "gtest/gtest.h"

#include "common/libwebm_utils.h"
#include "testing/test_util.h"

namespace {
class Webm2PesTests : public ::testing::Test {
 public:
  typedef std::vector<std::uint8_t> PesFileData;
  Webm2PesTests() {}
  bool CreateAndLoadTestInput() {
    libwebm::Webm2Pes converter(input_file_name_, temp_file_name_.name());
    EXPECT_TRUE(converter.ConvertToFile());
    pes_file_size_ = libwebm::test::GetFileSize(pes_file_name());
    EXPECT_GT(pes_file_size_, 0);
    pes_file_data_.reserve(pes_file_size_);
    EXPECT_EQ(pes_file_size_, pes_file_data_.capacity());
    libwebm::FilePtr file = libwebm::FilePtr(
        std::fopen(pes_file_name().c_str(), "rb"), libwebm::FILEDeleter());
    EXPECT_EQ(std::fread(&pes_file_data_[0], 1, pes_file_size_, file.get()),
              pes_file_size_);
    return true;
  }
  ~Webm2PesTests() = default;

  const std::string& pes_file_name() const { return temp_file_name_.name(); }
  std::uint64_t pes_file_size() const { return pes_file_size_; }
  const PesFileData& pes_file_data() const { return pes_file_data_; }

 private:
  const libwebm::test::TempFileDeleter temp_file_name_;
  const std::string input_file_name_ =
      libwebm::test::GetTestFilePath("bbb_480p_vp9_opus_1second.webm");
  std::uint64_t pes_file_size_ = 0;
  PesFileData pes_file_data_;
};

TEST_F(Webm2PesTests, CreatePesFile) {
  EXPECT_TRUE(CreateAndLoadTestInput());
}

TEST_F(Webm2PesTests, CanParseFirstPacket) {
  EXPECT_TRUE(CreateAndLoadTestInput());
}

}  // namespace

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
